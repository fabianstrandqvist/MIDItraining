#include "PluginProcessor.h"
#include <iostream>
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

    struct Scale
    {
        int      root = 0;
        uint16_t    bits = 0;

    }; // is this struct not pointless now, just bits would be enough for the highlights right?

    // The chord dictionary, written as interval formulas. The matcher returns the
    // first exact match, so order = priority.
    //
    // NOTE: matching is exact, so an extended chord is only recognised when *every*
    // note is present (no omitted root/5th yet — that's tier 3). And chords with
    // identical pitch-class sets (e.g. C6 == Am7, Csus2 == Gsus4) can't be told
    // apart without the bass note (tier 2). The "6" below is added so that
    // collision exists to test bass-note disambiguation against; until that rule
    // lands, first-match order decides (so {0,4,7,9} currently reads as a 6).
    constexpr ChordShape kChordShapes[] = {
        // triads
        { maskOf ({ 0, 4, 7 }),        "major"        },
        { maskOf ({ 0, 3, 7 }),        "minor"        },
        { maskOf ({ 0, 3, 6 }),        "diminished"   },
        { maskOf ({ 0, 4, 8 }),        "augmented"    },
        // sixths — same pcs as the min7 a third below (C6 == Am7); bass decides
        { maskOf ({ 0, 4, 7, 9 }),     "6"            },
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

    constexpr ChordShape kScales[] = {
        { maskOf({ 0, 2, 4, 5, 7, 9, 11 }), "major" },
        { maskOf({ 0, 2, 3, 5, 7, 8, 10 }), "minor" },
    };

    // need to add back all ambiguous chord shapes such as suspended etc. I will let Claude add this
    // demo by adding one clashing chord

    // Result of trying to name what's being held.
    struct ChordMatch
    {
        bool        found = false;
        int         root  = 0;
        const char* name  = "";
        int         slash = -1;
    };

    struct HeldChordNotes
    {
      uint16_t chord = 0;
      juce::String val;
      int bass;
    };

    // TODO: loop the 128 notes, test each bit in low/high, and for every held
    //       note set bit (note % 12) in the result.
    HeldChordNotes pitchClassSet (uint64_t low, uint64_t high)
    {
        uint16_t chord = 0;
        juce::String val;
        int bass = -1; // it does not have to be a bitmap right - maybe?

        for (int note = 0; note <= 127; ++note)
        {
            bool isHeld;
            if (note < 64)
                isHeld = low & (1ULL << note);
            else
                isHeld = high & (1ULL << (note - 64));

            if (isHeld)
            {
                if (bass == -1)
                {
                    bass = note % 12;
                }
                chord |= (1u << (note % 12)); // these are only the chord values, not shape yet
                val += juce::MidiMessage::getMidiNoteName (note, true, true, 3) + " ";
            }
        }
        return {chord, val, bass};
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

    // Match the pitch-class set against every chord shape at every root, and
    // return the one whose root IS the bass note -- that's what separates C6 from
    // Am7 when the notes are identical.
    //
    // Bug: Inversion do not work anymore
    ChordMatch identifyChord (uint16_t pcs, int bass)
    {
        std::vector<ChordMatch> chords;
        for (const auto& shape : kChordShapes)
            for (int root = 0; root < 12; ++root)
                if (pcs == rotl12 (shape.bits, root))
                    //  && root == bass
                    chords.push_back({true, root, shape.name});
                    // it would be safe to continue now right since one chord should not match several transposes?
        // could add so slash chords exist
        if (size(chords) == 1)
        {
            if (chords[0].root != bass)
                chords[0].slash = bass;
            return chords[0];

        } else if (size(chords) != 0)
        {
            for (int i = 0; i < size(chords); ++i)
            {
                if (chords[i].root == bass){
                    return chords[i];
                }
            }
            // here comes the problem, a transposed normal chord will not work as the bass note must not be in line with ... note
            // this is easy to solve if we only have one match but what if we have several, when could that be a problem?

        }
        //return { true, root, shape.name };

        return {};
    }
}

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{

    setSize (500, 300);

    midiVolume.setSliderStyle (juce::Slider::LinearBarVertical);
    midiVolume.setRange (0.0, 127.0, 1.0);
    midiVolume.setTextBoxStyle (juce::Slider::NoTextBox, false, 90, 0.0);
    midiVolume.setPopupDisplayEnabled (true, false, this);
    midiVolume.setTextValueSuffix ("Volume");
    midiVolume.setValue (1.0);

    // addAndMakeVisible (&midiVolume);
    addAndMakeVisible (highlightKeyboard);

    midiVolume.addListener (this);

    // Demo ComboBoxes
    addAndMakeVisible (scaleMenu);
    scaleMenu.addItem ("major", 1);
    scaleMenu.addItem ("minor", 2);
    scaleMenu.onChange = [this] { menuChanged(); };
    scaleMenu.setSelectedId (1);

    addAndMakeVisible (rootMenu);
    rootMenu.addItem ("C", 1);
    rootMenu.addItem ("C#", 2);
    rootMenu.onChange = [this] { menuChanged(); };
    rootMenu.setSelectedId (1);

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
  g.setFont (displayFont);

  auto [pcs, val, bass] = pitchClassSet (lastLow, lastHigh);
  ChordMatch m = identifyChord (pcs, bass);
  if (m.found){
    val += " - " + juce::MidiMessage::getMidiNoteName (m.root, true, false, 3) + " " + m.name;
    if (m.slash != -1)
      val += " / " + juce::MidiMessage::getMidiNoteName (m.slash, true, false, 3);

  }
  else if (val.isEmpty()){

    val = "No note playing";
  }
  g.drawFittedText (val, 0, 0, getWidth(), 30, juce::Justification::centred, 1);
}

void AudioPluginAudioProcessorEditor::resized()
{
    midiVolume.setBounds (40, 30, 20, getHeight() - 60);
    highlightKeyboard.setBounds (0, getHeight() - 80, getWidth(), 80);
    scaleMenu.setBounds (getWidth() - 100, 10, 90, 20);
    rootMenu.setBounds (getWidth() - 100, 40, 90, 20);
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

// TODO: update displayFont based on scaleMenu selection
// TODO: update rootMenu selection based on rootMenu selection
// Obviously needs a better way compared to having a case for each scale and root combination
// This might actually mask great to the chordshapes I have already used before
//
// The information should be used in two ways:
// 1. To inform ambiguous chord shapes
// 2. To visually update the MIDI display and highlight scale relevant notes if possible
void AudioPluginAudioProcessorEditor::menuChanged()
{
    std::cout << "scale: " << scaleMenu.getSelectedId() << " root: " << rootMenu.getSelectedId() << std::endl;


    glob_scale.root = rootMenu.getSelectedId() - 1;
    glob_scale.bits = rotl12(kScales[scaleMenu.getSelectedId() - 1].bits, glob_scale.root);
    highlightKeyboard.setHighlightScale(glob_scale.bits);


    // maybe have the Scale struct as a current member instead of using scaleState and rootState directly

    //scaleState = scaleState;
    //rootState = rootState;
    //


    // switch (scaleMenu.getSelectedId())
    // {
    //     case 1:
    //         displayFont.setStyleFlags (juce::Font::plain);
    //         break;
    //     case 2:
    //         displayFont.setStyleFlags (juce::Font::bold);
    //         break;
    //     default:
    //         break;
    // }
    //displayLabel.setFont (displayFont);
}
