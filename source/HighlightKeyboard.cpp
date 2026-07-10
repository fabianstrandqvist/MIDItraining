#include "HighlightKeyboard.h"
#include <random>

//TODO: Fill in keys based on a given scale
//Demoing with pink color and C-Maj as that does only necessitate white keys
void HighlightKeyboard::drawWhiteNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                                               bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour)
{
    juce::MidiKeyboardComponent::drawWhiteNote(midiNoteNumber, g, area, isDown, isOver, lineColour, textColour);
    juce::Colour s = juce::Colour::fromRGBA(255, 10, 205, 80);
    juce::Colour c = juce::Colour::fromRGBA(10, 50, 205, 80);

    if (scale & (1ULL << (midiNoteNumber % 12)))
    {
        g.setColour (s);
        g.fillRect (area);
    }

    if (midiNoteNumber < 64 && (stampedLow & (1ULL << midiNoteNumber)))
    {
        g.setColour (c);
        g.fillRect (area);
    } else if (midiNoteNumber >= 64 && (stampedHigh & (1ULL << (midiNoteNumber - 64))))
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

void HighlightKeyboard::setStampedChords(uint64_t low, uint64_t high)
{
    stampedLow = low;
    stampedHigh = high;
    repaint();
}
