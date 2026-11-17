#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

#include "Dsp.h"

class ChromaStackProcessor : public juce::AudioProcessor
{
public:
    ChromaStackProcessor();
    ~ChromaStackProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ChromaStack"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static const juce::StringArray& effectNames (int module);
    static const std::array<std::array<int, 4>, 24>& orders();
    static juce::StringArray orderNames();

    static constexpr const char* moduleIds[4] = { "char", "move", "diff", "tex" };
    static constexpr const char* moduleTitles[4] = { "CHARACTER", "MOVEMENT", "DIFFUSION", "TEXTURE" };

    juce::AudioProcessorValueTreeState apvts;

private:
    std::array<cs::Module, 4> modules;

    std::atomic<float>* pIn = nullptr;
    std::atomic<float>* pOut = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pDrift = nullptr;
    std::atomic<float>* pOrder = nullptr;
    std::array<std::atomic<float>*, 4> pOn {}, pFx {}, pA {}, pB {}, pModMix {};

    juce::SmoothedValue<float> inSm, outSm, mixSm, driftSm;
    std::array<juce::SmoothedValue<float>, 4> aSm, bSm, modMixSm;

    std::vector<float> dryL, dryR;
    int maxBlock = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChromaStackProcessor)
};
