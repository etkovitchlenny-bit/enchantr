#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

struct PresetInfo
{
    juce::String name;
    bool factory = false;
    int level = 1;
    juce::File file; // only for user presets
};

// shared between the synth voices so a new note can glide from the previous one
struct GlideShared
{
    std::atomic<float> prev { -1.0f };   // last note of earlier blocks
    std::atomic<float> newest { -1.0f }; // last note started
};

class EnchantrProcessor : public juce::AudioProcessor
{
public:
    static constexpr int scopeSize = 512;

    EnchantrProcessor();
    ~EnchantrProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Enchantr"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // sample + level up
    bool loadSample (const juce::File& file, bool preview = true);
    void levelUp();
    void requestPreview() { previewPending = true; }
    int getLevel() const { return level.load(); }
    bool hasSample() const { return sampleLoaded.load(); }
    juce::String getSampleName() const { return sampleName; }

    // presets
    const std::vector<PresetInfo>& getPresets() const { return presets; }
    void refreshPresets();
    bool loadPreset (int index);
    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (int index);
    juce::String getPresetName() const;
    int getCurrentPresetIndex() const;
    static juce::File getPresetFolder();

    // faint waveform for the UI
    int getScopeHead() const { return scopeHead.load(); }
    float getScopeSample (int i) const { return scope[(size_t) i].load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateEq (bool force);
    void setParamReal (const char* id, float realValue);
    void resetToDefaults (bool keepRoot);
    void setLevel (int newLevel);

    juce::Synthesiser synth;
    juce::AudioFormatManager formatManager;
    GlideShared glideShared;

    double currentSampleRate = 44100.0;

    // effects
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> tapeDelay;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> echoDelay;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;
    juce::dsp::IIR::Filter<float> airHp[2], eqLowF[2], eqMidF[2], eqHighF[2];

    float tapePhase1 = 0.f, tapePhase2 = 0.f, tremPhase = 0.f;
    float tapeLp[2] { 0.f, 0.f };
    float echoLp[2] { 0.f, 0.f };
    float lastLo = 1e9f, lastMid = 1e9f, lastMidF = 1e9f, lastHi = 1e9f;

    juce::LinearSmoothedValue<float> smSat, smTape, smTrem, smDelay, smDelayTime, smMix, smOut;
    juce::AudioBuffer<float> dryBuf;

    // parameter pointers
    std::atomic<float> *pSat, *pTape, *pChorus, *pTrem, *pDelay, *pReverb;
    std::atomic<float> *pTremRate, *pDelayTime, *pDelayFb, *pRevSize;
    std::atomic<float> *pWarmth, *pAir, *pWidth, *pEqLow, *pEqMid, *pEqMidFreq, *pEqHigh;
    std::atomic<float> *pMix, *pOut, *pAttack, *pRelease, *pRoot;

    // state
    std::atomic<int> level { 0 };
    std::atomic<bool> sampleLoaded { false };
    std::atomic<bool> previewPending { false };
    int previewSamplesLeft = 0;
    int previewNote = 60;
    juce::String sampleName;
    juce::Random rng;
    std::vector<std::array<float, 17>> rollHistory; // recent level-up rolls (normalised)
    std::vector<PresetInfo> presets;

    std::array<std::atomic<float>, scopeSize> scope;
    std::atomic<int> scopeHead { 0 };
    int scopeCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnchantrProcessor)
};
