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

    

    uint64_t lastLow = 0;

    uint64_t lastHigh = 0;

    const uint16_t major_mask = 0x091;

    juce::Slider midiVolume;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
