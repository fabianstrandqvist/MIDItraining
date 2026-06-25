#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Chord-identification logic. Pure functions over note state — no Graphics,
// no editor members. This whole block could be lifted into a test without a GUI.
namespace
{
    // Build a 12-bit pitch-class mask from a chord's interval formula (semitones
    // above the root). Computed at compile time, so the dictionary below reads as
    // musical formulas instead of hand-typed hex — adding a chord is now one line.
    constexpr uint16_t maskOf (std::initializer_list<int> intervals)
    {
        uint16_t bits = 0;
        for (int i : intervals)
            bits |= static_cast<uint16_t> (1u << i);
        return bits;
    }

    // A named chord "shape": the pitch-class set for that chord rooted at C.
    struct ChordShape
    {
        uint16_t    bits;
        const char* name;
    };

    // The chord dictionary, written as interval formulas. The matcher returns the
    // first exact match, so order = priority.
    //
    // NOTE: matching is exact, so an extended chord is only recognised when *every*
    // note is present (no omitted root/5th yet — that's tier 3). And chords with
    // identical pitch-class sets (e.g. C6 == Am7, Csus2 == Gsus4) can't be told
    // apart without the bass note (tier 2), which is why 6th/sus chords are left
    // out for now rather than fighting over the lowest root.
    constexpr ChordShape kChordShapes[] = {
        // triads
        { maskOf ({ 0, 4, 7 }),        "major"        },
        { maskOf ({ 0, 3, 7 }),        "minor"        },
        { maskOf ({ 0, 3, 6 }),        "diminished"   },
        { maskOf ({ 0, 4, 8 }),        "augmented"    },
        // sevenths
        { maskOf ({ 0, 4, 7, 10 }),    "dominant 7"   },
        { maskOf ({ 0, 4, 7, 11 }),    "major 7"      },
        { maskOf ({ 0, 3, 7, 10 }),    "minor 7"      },
        { maskOf ({ 0, 3, 6, 10 }),    "half-dim 7"   },
        { maskOf ({ 0, 3, 6, 9 }),     "diminished 7" },
        // extensions (full voicings only) — add more here as one-liners
        { maskOf ({ 0, 2, 4, 7, 11 }), "major 9"      },
        { maskOf ({ 0, 2, 4, 7, 10 }), "dominant 9"   },
        { maskOf ({ 0, 2, 3, 7, 10 }), "minor 9"      },
        { maskOf ({ 0, 2, 4, 5, 7, 11 }), "major 11"  },
    };

    // Result of trying to name what's being held.
    struct ChordMatch
    {
        bool        found = false;
        int         root  = 0;
        const char* name  = "";
    };

    struct HeldChordNotes
    {
      uint16_t chord = 0;
      juce::String val;
    };

    // Collapse the 128-note held state (low = notes 0..63, high = 64..127)
    // into a single 12-bit pitch-class set, one bit per pitch class.
    //
    // TODO: loop the 128 notes, test each bit in low/high, and for every held
    //       note set bit (note % 12) in the result.
    HeldChordNotes pitchClassSet (uint64_t low, uint64_t high)
    {
        uint16_t chord = 0;
        juce::String val;

        for (int note = 0; note <= 127; ++note) 
        {
            bool isHeld;
            if (note < 64)
                isHeld = low & (1ULL << note);
            else
                isHeld = high & (1ULL << (note - 64));

            if (isHeld)
            {
                chord |= (1u << (note % 12)); // these are only the chord values, not shape yet
                val += juce::MidiMessage::getMidiNoteName (note, true, true, 3) + " ";
            }
        }
        return {chord, val};
    }

    // Circular-rotate a 12-bit value left by `by` places, wrapping bits that
    // fall off bit 11 back around to bit 0.
    //
    // TODO: shift left by `by`, OR in the overflow, then keep only the low 12
    //       bits. Think about which constant masks off "the low 12 bits".
    uint16_t rotl12 (uint16_t bits, int by)
    {
        // all bits are first shifted by 'by', this is ok due to integer promotion
        // but we want bits that are shifted outside the 12 bits to rotated
        // this is done by using OR (shift bits to right by 12 - 'by')
        // the result is a bitmap that has both the too much shifted bits to the left
        // so it is masked with AND (12 bits as ones)
        return ((bits << by) | (bits >> (12 - by))) & 0xFFF;

    }

    // Match the pitch-class set against every chord shape at every root.
    // Outer loop walks the dictionary in priority order; inner loop tries the
    // 12 possible roots for that shape. First exact match wins.
    ChordMatch identifyChord (uint16_t pcs)
    {
        for (const auto& shape : kChordShapes)
            for (int root = 0; root < 12; ++root)
                if (pcs == rotl12 (shape.bits, root))
                    return { true, root, shape.name };

        return {};
    }
}

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{

    setSize (200, 200);

    midiVolume.setSliderStyle (juce::Slider::LinearBarVertical);
    midiVolume.setRange (0.0, 127.0, 1.0);
    midiVolume.setTextBoxStyle (juce::Slider::NoTextBox, false, 90, 0.0);
    midiVolume.setPopupDisplayEnabled (true, false, this);
    midiVolume.setTextValueSuffix ("Volume");
    midiVolume.setValue (1.0);

    addAndMakeVisible (&midiVolume);

    midiVolume.addListener (this);

    startTimerHz (30);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
  g.fillAll (juce::Colours::white);
  g.setColour (juce::Colours::black);
  g.setFont (15.0f);

  auto [pcs, val] = pitchClassSet (lastLow, lastHigh);
  ChordMatch m = identifyChord (pcs);
  if (m.found){
    val += " - " + juce::MidiMessage::getMidiNoteName (m.root, true, false, 3) + " " + m.name;
  }
  else if (val.isEmpty()){
    
    val = "No note playing";
  }
  g.drawFittedText (val, 0, 0, getWidth(), 30, juce::Justification::centred, 1);
}

void AudioPluginAudioProcessorEditor::resized()
{
    midiVolume.setBounds (40, 30, 20, getHeight() - 60);
}

void AudioPluginAudioProcessorEditor::sliderValueChanged (juce::Slider* slider){
  processorRef.noteOnVel = midiVolume.getValue();
}

void AudioPluginAudioProcessorEditor::timerCallback()
{
  auto stateLow = processorRef.getStateLow();
  auto stateHigh = processorRef.getStateHigh();


  if (stateLow != lastLow || stateHigh != lastHigh){
    lastLow = stateLow;
    lastHigh = stateHigh;
    repaint();
  }
}

