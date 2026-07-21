#include "PluginProcessor.h"
#include <fcntl.h>
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
    // TODO: Possibly a more dynamic approach, for example major 7, major 9 etc feels wasteful to have separate entries for
    constexpr ChordShape kChordShapes[] = {
        // triads
        { maskOf ({ 0, 4, 7 }),        "major"        },
        { maskOf ({ 0, 3, 7 }),        "minor"        },
        { maskOf ({ 0, 2, 7 }), "sus2" },
        { maskOf ({ 0, 5, 7 }), "sus4" },
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
    ChordMatch identifyChord (uint16_t pcs, int bass, int tonic)
    {
        std::vector<ChordMatch> chords;
        for (const auto& shape : kChordShapes)
            for (int root = 0; root < 12; ++root)
                if (pcs == rotl12 (shape.bits, root))
                    //  && root == bass
                    chords.push_back({true, root, shape.name});
                    // it would be safe to continue now right since one chord should not match several transposes?

        if (size(chords) == 1)
        {
            if (chords[0].root != bass)
                chords[0].slash = bass;
            return chords[0];

        } else if (size(chords) != 0)
        {
            std::vector<int> scores(size(chords));
            for (int i = 0; i < size(chords); ++i)
            {
                scores[i] = 0;
                if (chords[i].root == bass){
                    // can also add slash maybe to visuals
                    scores[i] += 2;
                } else {
                    chords[i].slash = bass;
                }

                if (chords[i].root == tonic)
                {
                    scores[i] += 1;
                }
            }
            int best = max_element(begin(scores), end(scores)) - begin(scores);

            return chords[best];

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

    static const juce::StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    addAndMakeVisible (rootMenu);
    for (int i = 0; i < noteNames.size(); ++i)
        rootMenu.addItem (noteNames[i], i + 1); // JUCE menu IDs start at 1

    rootMenu.onChange = [this] { menuChanged(); };
    rootMenu.setSelectedId (1);

    addAndMakeVisible (stampButton);
    stampButton.onClick = [this] { highlightKeyboard.setStampedChords (lastLow, lastHigh);
        processorRef.notifyStamping(currentChordName);
        repaint();
    };

    addAndMakeVisible (clearButton);
    clearButton.onClick = [this] { highlightKeyboard.setStampedChords (0, 0);
        processorRef.clearStamping();
        repaint();
    };

    addAndMakeVisible (playButton);
    playButton.onClick = [this] { highlightKeyboard.setStampedChords(processorRef.getStampedChord().stateLow, processorRef.getStampedChord().stateHigh);
        playMode = true;

        repaint();
    }; //is this dumb?

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


  g.drawFittedText (currentChordName, 0, 0, getWidth(), 30, juce::Justification::centred, 1);

  g.drawFittedText (processorRef.getSequence(), 0, 30, getWidth(), 30, juce::Justification::centred, 1); // does stamping trigger a repaint anywhere?

}

void AudioPluginAudioProcessorEditor::resized()
{
    midiVolume.setBounds (40, 30, 20, getHeight() - 60);
    highlightKeyboard.setBounds (0, getHeight() - 80, getWidth(), 80);
    scaleMenu.setBounds (getWidth() - 100, 10, 90, 20);
    rootMenu.setBounds (getWidth() - 100, 40, 90, 20);
    stampButton.setBounds (getWidth() - 100, 70, 90, 20);
    clearButton.setBounds (getWidth() - 100, 100, 90, 20);
    playButton.setBounds (getWidth() - 100, 130, 90, 20);
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

    updateChordName();

    if (playMode){
      if (processorRef.isChordCorrect(stateLow, stateHigh)){
        highlightKeyboard.setStampedChords(processorRef.getStampedChord().stateLow, processorRef.getStampedChord().stateHigh);
        // should have a better signifier for having played the correct chord
      }
    }

  }
}

void AudioPluginAudioProcessorEditor::updateChordName()
{
    auto [pcs, val, bass] = pitchClassSet (lastLow, lastHigh);
    ChordMatch m = identifyChord (pcs, bass, glob_scale.root);
    if (m.found){
      val = juce::MidiMessage::getMidiNoteName (m.root, true, false, 3) + " " + m.name;
      if (m.slash != -1)
        val += " / " + juce::MidiMessage::getMidiNoteName (m.slash, true, false, 3);

    }
    else if (val.isEmpty()){

      val = "No note playing";
    }

    currentChordName = val;
    repaint();
}

// TODO: update displayFont based on scaleMenu selection
// TODO: update rootMenu selection based on rootMenu selection
// Obviously needs a better way compared to having a case for each scale and root combination
// This might actually mask great to the chordshapes I have already used before
//
// The information should be used in two ways:
// 1. To inform ambiguous chord shapes - not done
// 2. To visually update the MIDI display and highlight scale relevant notes if possible - done
void AudioPluginAudioProcessorEditor::menuChanged()
{
    glob_scale.root = rootMenu.getSelectedId() - 1;
    glob_scale.bits = rotl12(kScales[scaleMenu.getSelectedId() - 1].bits, glob_scale.root);
    highlightKeyboard.setHighlightScale(glob_scale.bits);

    updateChordName();
}
