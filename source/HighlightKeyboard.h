#pragma once
#include <cstdint>
#include <juce_audio_utils/juce_audio_utils.h>

class HighlightKeyboard : public juce::MidiKeyboardComponent
{
public:
    using juce::MidiKeyboardComponent::MidiKeyboardComponent;

    void setHighlightScale(uint16_t scale);




private:
    //==============================================================================
    void drawWhiteNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                                               bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote(int /*midiNoteNumber*/, juce::Graphics& g, juce::Rectangle<float> area,
                                               bool isDown, bool isOver, juce::Colour noteFillColour) override;

    uint16_t scale = 0; // the initial state will be wrong but this is ok for now




};
