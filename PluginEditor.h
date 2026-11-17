#pragma once

#include "PluginProcessor.h"

using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

inline juce::Colour moduleColour (int m)
{
    static const juce::Colour c[4] = { juce::Colour (0xffe0533d), juce::Colour (0xff3da5e0),
                                       juce::Colour (0xff8a5be0), juce::Colour (0xff3dc97a) };
    return c[juce::jlimit (0, 3, m)];
}

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId,
          const juce::String& name, juce::Colour colour)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
        slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (slider);
        addAndMakeVisible (label);
        att = std::make_unique<SliderAtt> (s, paramId, slider);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (16));
        slider.setBounds (r);
    }

private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<SliderAtt> att;
};

class ModulePanel : public juce::Component
{
public:
    ModulePanel (juce::AudioProcessorValueTreeState& s, int m);
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    int index;
    juce::Colour colour;
    juce::String title;
    juce::ToggleButton on { "ON" };
    juce::ComboBox fx;
    Knob a, b, mix;
    std::unique_ptr<ButtonAtt> onAtt;
    std::unique_ptr<ComboAtt> fxAtt;
};

class ChromaStackEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChromaStackEditor (ChromaStackProcessor&);
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    ChromaStackProcessor& proc;
    Knob inK, outK, mixK, driftK;
    juce::ComboBox orderBox;
    std::unique_ptr<ComboAtt> orderAtt;
    std::array<std::unique_ptr<ModulePanel>, 4> panels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChromaStackEditor)
};
