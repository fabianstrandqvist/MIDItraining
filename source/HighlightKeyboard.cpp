#include "HighlightKeyboard.h"

//TODO: Fill in keys based on a given scale
//Demoing with pink color and C-Maj as that does only necessitate white keys
void HighlightKeyboard::drawWhiteNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                                               bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour)
{
    juce::MidiKeyboardComponent::drawWhiteNote(midiNoteNumber, g, area, isDown, isOver, lineColour, textColour);
    juce::Colour c = juce::Colour::fromRGBA(255, 10, 205, 100);

    if (scale & (1ULL << (midiNoteNumber % 12)))
    {
        g.setColour (c);
        g.fillRect (area);
    }



}

void HighlightKeyboard::drawBlackNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                                               bool isDown, bool isOver, juce::Colour noteFillColour)
{
    juce::MidiKeyboardComponent::drawBlackNote(midiNoteNumber, g, area, isDown, isOver, noteFillColour);

    if (scale & (1ULL << (midiNoteNumber % 12)))
    {
        g.setColour (juce::Colour::fromRGBA(255, 10, 205, 100));
        g.fillRect (area);
    }
}

void HighlightKeyboard::setHighlightScale(uint16_t newScale)
{
    scale = newScale;
    repaint();
}
