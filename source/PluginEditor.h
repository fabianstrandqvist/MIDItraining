#pragma once

#include "PluginProcessor.h"

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

    uint64_t lastLow = 0;
    uint64_t lastHigh = 0;

    const uint16_t major_mask = 0x091;
    const uint16_t minor_mask = 0x060; // is this correct?

    uint16_t rootState = 0;
    uint16_t scaleState = 0;

    juce::ComboBox scaleMenu;
    juce::ComboBox rootMenu;

    void menuChanged();
    juce::Font displayFont { 15.0f };

    juce::Slider midiVolume;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
