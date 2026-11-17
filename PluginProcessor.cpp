#include "PluginProcessor.h"

#include "PluginEditor.h"

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

const juce::StringArray& ChromaStackProcessor::effectNames (int module)
{
    static const juce::StringArray names[4] = {
        { "Overdrive", "Fuzz", "Compressor", "Tape Sat", "Swell" },
        { "Tremolo", "Vibrato", "Phaser", "Doubler", "Chorus" },
        { "Echo", "Reverb", "Tape Delay", "Room", "Smear" },
        { "Bit Crush", "Lo-Fi", "Crackle", "Filter", "Static" }
    };
    return names[juce::jlimit (0, 3, module)];
}

const std::array<std::array<int, 4>, 24>& ChromaStackProcessor::orders()
{
    static const auto table = []
    {
        std::array<std::array<int, 4>, 24> t {};
        std::array<int, 4> p { 0, 1, 2, 3 };
        size_t i = 0;
        do { t[i++] = p; } while (std::next_permutation (p.begin(), p.end()));
        return t;
    }();
    return table;
}

juce::StringArray ChromaStackProcessor::orderNames()
{
    const char letters[4] = { 'C', 'M', 'D', 'T' };
    juce::StringArray names;
    for (auto& perm : orders())
    {
        juce::String s;
        for (int i = 0; i < 4; ++i)
            s << (i ? " > " : "") << juce::String::charToString ((juce::juce_wchar) letters[perm[(size_t) i]]);
        names.add (s);
    }
    return names;
}

Layout ChromaStackProcessor::createLayout()
{
    using P = juce::ParameterID;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        P { "in", 1 }, "Input", juce::NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        P { "out", 1 }, "Output", juce::NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        P { "mix", 1 }, "Mix", juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 1.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        P { "drift", 1 }, "Drift", juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (P { "order", 1 }, "Order", orderNames(), 0));

    const bool defOn[4] = { false, true, true, false };
    const float defMix[4] = { 0.5f, 0.5f, 0.3f, 0.5f };
    for (int m = 0; m < 4; ++m)
    {
        const juce::String id = moduleIds[m];
        const juce::String title = juce::String (moduleTitles[m]).toLowerCase();
        params.push_back (std::make_unique<juce::AudioParameterBool> (P { id + "_on", 1 }, title + " on", defOn[m]));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (P { id + "_fx", 1 }, title + " effect", effectNames (m), 0));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            P { id + "_a", 1 }, title + " A", juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            P { id + "_b", 1 }, title + " B", juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            P { id + "_mix", 1 }, title + " mix", juce::NormalisableRange<float> (0.f, 1.f, 0.001f), defMix[m]));
    }

    return { params.begin(), params.end() };
}

ChromaStackProcessor::ChromaStackProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int m = 0; m < 4; ++m)
    {
        cs::buildModule (modules[(size_t) m], m);
        const juce::String id = moduleIds[m];
        pOn[(size_t) m] = apvts.getRawParameterValue (id + "_on");
        pFx[(size_t) m] = apvts.getRawParameterValue (id + "_fx");
        pA[(size_t) m] = apvts.getRawParameterValue (id + "_a");
        pB[(size_t) m] = apvts.getRawParameterValue (id + "_b");
        pModMix[(size_t) m] = apvts.getRawParameterValue (id + "_mix");
    }
    pIn = apvts.getRawParameterValue ("in");
    pOut = apvts.getRawParameterValue ("out");
    pMix = apvts.getRawParameterValue ("mix");
    pDrift = apvts.getRawParameterValue ("drift");
    pOrder = apvts.getRawParameterValue ("order");
}

void ChromaStackProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = std::max (samplesPerBlock, 64);
    dryL.assign ((size_t) maxBlock, 0.f);
    dryR.assign ((size_t) maxBlock, 0.f);

    for (auto& m : modules) m.prepare (sampleRate, maxBlock);

    auto init = [sampleRate] (juce::SmoothedValue<float>& s, float v)
    {
        s.reset (sampleRate, 0.05);
        s.setCurrentAndTargetValue (v);
    };
    init (inSm, juce::Decibels::decibelsToGain (pIn->load()));
    init (outSm, juce::Decibels::decibelsToGain (pOut->load()));
    init (mixSm, pMix->load());
    init (driftSm, pDrift->load());
    for (size_t m = 0; m < 4; ++m)
    {
        init (aSm[m], pA[m]->load());
        init (bSm[m], pB[m]->load());
        init (modMixSm[m], pModMix[m]->load());
    }
}

bool ChromaStackProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ChromaStackProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || total == 0) return;

    inSm.setTargetValue (juce::Decibels::decibelsToGain (pIn->load()));
    outSm.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    mixSm.setTargetValue (pMix->load());
    driftSm.setTargetValue (pDrift->load());
    for (size_t m = 0; m < 4; ++m)
    {
        aSm[m].setTargetValue (pA[m]->load());
        bSm[m].setTargetValue (pB[m]->load());
        modMixSm[m].setTargetValue (pModMix[m]->load());
    }

    const int orderIdx = juce::jlimit (0, 23, (int) std::lround (pOrder->load()));
    const auto& order = orders()[(size_t) orderIdx];

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    for (int start = 0; start < total; start += maxBlock)
    {
        const int len = std::min (maxBlock, total - start);
        float* l = L + start;
        float* r = R + start;

        // giriş kazancı
        for (int i = 0; i < len; ++i)
        {
            const float g = inSm.getNextValue();
            l[i] *= g;
            r[i] *= g;
        }
        std::copy (l, l + len, dryL.begin());
        std::copy (r, r + len, dryR.begin());

        const float drift = driftSm.skip (len);

        for (int slot = 0; slot < 4; ++slot)
        {
            const size_t m = (size_t) order[(size_t) slot];
            // yumuşatıcıları her zaman ilerlet
            const float a = aSm[m].skip (len);
            const float b = bSm[m].skip (len);
            const float mm = modMixSm[m].skip (len);
            modules[m].process (l, r, len,
                                (int) std::lround (pFx[m]->load()),
                                pOn[m]->load() > 0.5f,
                                a, b, mm, drift);
        }

        // global mix + çıkış kazancı
        for (int i = 0; i < len; ++i)
        {
            const float mx = mixSm.getNextValue();
            const float og = outSm.getNextValue();
            l[i] = (dryL[(size_t) i] * (1.f - mx) + l[i] * mx) * og;
            r[i] = (dryR[(size_t) i] * (1.f - mx) + r[i] * mx) * og;
        }
    }
}

juce::AudioProcessorEditor* ChromaStackProcessor::createEditor()
{
    return new ChromaStackEditor (*this);
}

void ChromaStackProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ChromaStackProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChromaStackProcessor();
}
