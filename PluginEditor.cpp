#include "PluginEditor.h"

ModulePanel::ModulePanel (juce::AudioProcessorValueTreeState& s, int m)
    : index (m),
      colour (moduleColour (m)),
      title (ChromaStackProcessor::moduleTitles[m]),
      a (s, juce::String (ChromaStackProcessor::moduleIds[m]) + "_a", "A", moduleColour (m)),
      b (s, juce::String (ChromaStackProcessor::moduleIds[m]) + "_b", "B", moduleColour (m)),
      mix (s, juce::String (ChromaStackProcessor::moduleIds[m]) + "_mix", "MIX", moduleColour (m))
{
    const juce::String id = ChromaStackProcessor::moduleIds[m];

    on.setColour (juce::ToggleButton::tickColourId, colour);
    addAndMakeVisible (on);
    onAtt = std::make_unique<ButtonAtt> (s, id + "_on", on);

    fx.addItemList (ChromaStackProcessor::effectNames (m), 1);
    addAndMakeVisible (fx);
    fxAtt = std::make_unique<ComboAtt> (s, id + "_fx", fx);

    addAndMakeVisible (a);
    addAndMakeVisible (b);
    addAndMakeVisible (mix);
}

void ModulePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (4.f);
    g.setColour (colour.withAlpha (0.12f));
    g.fillRoundedRectangle (r, 8.f);
    g.setColour (colour.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 8.f, 1.5f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.f, juce::Font::bold));
    g.drawText (title, getLocalBounds().reduced (14, 10).removeFromTop (22),
                juce::Justification::centredLeft);
}

void ModulePanel::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    auto header = r.removeFromTop (24);
    on.setBounds (header.removeFromRight (56));
    r.removeFromTop (6);
    fx.setBounds (r.removeFromTop (28));
    r.removeFromTop (10);
    auto top = r.removeFromTop (r.getHeight() / 2);
    a.setBounds (top.removeFromLeft (top.getWidth() / 2));
    b.setBounds (top);
    mix.setBounds (r.reduced (r.getWidth() / 4, 0));
}

ChromaStackEditor::ChromaStackEditor (ChromaStackProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      inK (p.apvts, "in", "INPUT", juce::Colours::lightgrey),
      outK (p.apvts, "out", "OUTPUT", juce::Colours::lightgrey),
      mixK (p.apvts, "mix", "MIX", juce::Colours::lightgrey),
      driftK (p.apvts, "drift", "DRIFT", juce::Colour (0xffffc247))
{
    addAndMakeVisible (inK);
    addAndMakeVisible (outK);
    addAndMakeVisible (mixK);
    addAndMakeVisible (driftK);

    orderBox.addItemList (ChromaStackProcessor::orderNames(), 1);
    addAndMakeVisible (orderBox);
    orderAtt = std::make_unique<ComboAtt> (p.apvts, "order", orderBox);

    for (int m = 0; m < 4; ++m)
    {
        panels[(size_t) m] = std::make_unique<ModulePanel> (p.apvts, m);
        addAndMakeVisible (*panels[(size_t) m]);
    }

    setSize (780, 500);
}

void ChromaStackEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171c));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (22.f, juce::Font::bold));
    g.drawText ("CHROMASTACK", 20, 12, 260, 30, juce::Justification::centredLeft);
    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (12.f));
    g.drawText ("SIGNAL ORDER  (C=Character  M=Movement  D=Diffusion  T=Texture)", 20, 48, 420, 16,
                juce::Justification::centredLeft);
}

void ChromaStackEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (130);

    auto left = top.removeFromLeft (300).reduced (20, 0);
    orderBox.setBounds (left.getX(), 70, 200, 28);

    const int kw = top.getWidth() / 4;
    inK.setBounds (top.removeFromLeft (kw).reduced (4));
    outK.setBounds (top.removeFromLeft (kw).reduced (4));
    mixK.setBounds (top.removeFromLeft (kw).reduced (4));
    driftK.setBounds (top.reduced (4));

    const int pw = r.getWidth() / 4;
    for (int m = 0; m < 4; ++m)
        panels[(size_t) m]->setBounds (r.removeFromLeft (pw).reduced (2));
}
