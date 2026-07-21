#pragma once

#include "PluginProcessor.h"
#include "HighlightKeyboard.h"

struct Scale
{
    int root = 0;
    uint16_t bits = 0;
};

//==============================================================================
class AudioPluginAudioProcessorEditor final : public
juce::AudioProcessorEditor,
  private
juce::Slider::Listener,
  private
juce::Timer
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void sliderValueChanged (juce::Slider* slider) override;
    AudioPluginAudioProcessor& processorRef;
    void timerCallback() override;

    juce::MidiKeyboardComponent keyboard { processorRef.keyboardState,
                                           juce::MidiKeyboardComponent::horizontalKeyboard };



    HighlightKeyboard highlightKeyboard { processorRef.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };


    uint64_t lastLow = 0;
    uint64_t lastHigh = 0;

    uint16_t rootState = 0;
    uint16_t scaleState = 0;

    Scale glob_scale {};

    // should this necessarily be a juce::String?
    juce::String sequence { "" };

    juce::ComboBox scaleMenu;
    juce::ComboBox rootMenu;
    juce::TextButton stampButton { "Capture" };
    juce::TextButton clearButton { "Clear" };
    juce::TextButton playButton { "Play" }; //should start the stamped sequence, highlighting the chord to be played

    bool playMode = false;


    juce::String currentChordName { "No note playing" };

    void updateChordName();
    void menuChanged();

    juce::Font displayFont { 15.0f };

    juce::Slider midiVolume;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
