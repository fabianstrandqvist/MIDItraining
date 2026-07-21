#pragma once

#include <cstdint>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include <utility>

struct StampedChord
{
    uint64_t stateLow;
    uint64_t stateHigh;
    juce::String name;
};

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    float noteOnVel;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    uint64_t getStateHigh() const { return state_high.load(); }

    uint64_t getStateLow() const { return state_low.load(); }
    juce::MidiKeyboardState keyboardState;

    void notifyStamping(juce::String chordName);
    void clearStamping();

    juce::String getSequence() const;
    StampedChord getStampedChord() const;

    bool isChordCorrect(uint64_t stateLow, uint64_t stateHigh);




private:
    //==============================================================================
    std::atomic<uint64_t> state_high { 0 };

    std::atomic<uint64_t> state_low { 0 };

    std::vector<StampedChord> stampedChords;

    int currentChord = 0;

    // I guess that the playing sequence logic can be handled entirely in the editor since we have bitmaps and the target chord state
    // Although need to be informed that play mode is on
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
