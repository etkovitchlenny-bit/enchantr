#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    class EnchantrSound : public juce::SynthesiserSound
    {
    public:
        EnchantrSound (juce::AudioBuffer<float>&& b, double sr) : data (std::move (b)), fileSampleRate (sr) {}
        bool appliesToNote (int) override { return true; }
        bool appliesToChannel (int) override { return true; }

        juce::AudioBuffer<float> data;
        double fileSampleRate;
    };

    class EnchantrVoice : public juce::SynthesiserVoice
    {
    public:
        EnchantrVoice (juce::AudioProcessorValueTreeState& s, GlideShared& g) : apvts (s), shared (g) {}

        bool canPlaySound (juce::SynthesiserSound* s) override
        {
            return dynamic_cast<EnchantrSound*> (s) != nullptr;
        }

        void startNote (int midiNote, float velocity, juce::SynthesiserSound* s, int) override
        {
            sound = static_cast<EnchantrSound*> (s);
            pos = 0.0;
            gain = 0.35f + 0.65f * velocity;

            root = apvts.getRawParameterValue ("root")->load();
            target = (float) midiNote;

            const float glide = apvts.getRawParameterValue ("glide")->load();
            vibDepth = apvts.getRawParameterValue ("vibrato")->load();
            const float prev = shared.prev.load();
            cur = (glide > 0.001f && prev >= 0.0f) ? prev : target;
            shared.newest.store (target);

            const double sr = getSampleRate();
            const float glideSec = 0.01f + glide * glide * 0.8f;
            glideCoef = 1.0f - std::exp (-1.0f / (glideSec * (float) sr));
            fileRatio = sound->fileSampleRate / sr;

            vibPhase = 0.0f;
            vibAge = 0.0f;
            dynamicPitch = (cur != target) || vibDepth > 0.001f;
            ratio = fileRatio * std::exp2 (((double) cur - (double) root) / 12.0);

            adsr.setSampleRate (sr);
            adsr.setParameters ({ apvts.getRawParameterValue ("attack")->load(), 0.0f, 1.0f,
                                  apvts.getRawParameterValue ("release")->load() });
            adsr.noteOn();
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff)
            {
                adsr.noteOff();
            }
            else
            {
                adsr.reset();
                sound = nullptr;
                clearCurrentNote();
            }
        }

        void pitchWheelMoved (int) override {}
        void controllerMoved (int, int) override {}

        void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
        {
            if (sound == nullptr)
                return;

            auto& d = sound->data;
            const int n = d.getNumSamples();
            const int dc = d.getNumChannels();
            const float sr = (float) getSampleRate();

            for (int i = 0; i < num; ++i)
            {
                if (! adsr.isActive() || pos >= (double) (n - 2))
                {
                    sound = nullptr;
                    clearCurrentNote();
                    break;
                }

                if (dynamicPitch)
                {
                    if (cur != target)
                    {
                        cur += (target - cur) * glideCoef;
                        if (std::abs (target - cur) < 0.002f)
                            cur = target;
                    }

                    float semis = cur - root;
                    if (vibDepth > 0.001f)
                    {
                        vibAge += 1.0f / sr;
                        vibPhase += twoPi * 5.5f / sr;
                        if (vibPhase >= twoPi) vibPhase -= twoPi;
                        semis += vibDepth * 0.6f * juce::jmin (1.0f, vibAge / 0.2f) * std::sin (vibPhase);
                    }
                    ratio = fileRatio * std::exp2 ((double) semis / 12.0);
                }

                const int p0 = (int) pos;
                const float f = (float) (pos - (double) p0);
                const float tail = juce::jmin (1.0f, (float) (((double) (n - 2) - pos) / 64.0));
                const float env = adsr.getNextSample() * gain * tail;

                for (int ch = 0; ch < out.getNumChannels(); ++ch)
                {
                    const int src = ch < dc ? ch : 0;
                    const float s = d.getSample (src, p0) * (1.0f - f) + d.getSample (src, p0 + 1) * f;
                    out.addSample (ch, start + i, s * env);
                }

                pos += ratio;
            }
        }

    private:
        juce::AudioProcessorValueTreeState& apvts;
        GlideShared& shared;
        juce::ReferenceCountedObjectPtr<EnchantrSound> sound;
        juce::ADSR adsr;
        double pos = 0.0, ratio = 1.0, fileRatio = 1.0;
        float gain = 1.0f, root = 60.0f, target = 60.0f, cur = 60.0f, glideCoef = 1.0f;
        float vibDepth = 0.0f, vibPhase = 0.0f, vibAge = 0.0f;
        bool dynamicPitch = false;
    };

    inline float softClip (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.8f)
            return x;
        const float s = x < 0.f ? -1.f : 1.f;
        return s * (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f));
    }

    // parameters that Level Up rolls, in the order used for the "never the same" history
    const char* const rolledIds[17] = { "sat", "tape", "chorus", "trem", "delay", "reverb",
                                        "tremRate", "delayTime", "delayFb", "reverbSize",
                                        "warmth", "air", "width", "eqLow", "eqMid", "eqMidFreq", "eqHigh" };

    struct FactoryPreset
    {
        const char* name;
        int level;
        std::vector<std::pair<const char*, float>> v;
    };

    const std::vector<FactoryPreset>& factoryPresets()
    {
        static const std::vector<FactoryPreset> list = {
            { "SPARKLE RAIN", 3, { { "sat", 0.15f }, { "tape", 0.10f }, { "chorus", 0.55f }, { "delay", 0.35f }, { "reverb", 0.50f },
                                   { "delayTime", 180.f }, { "delayFb", 0.30f }, { "reverbSize", 0.70f }, { "air", 0.50f },
                                   { "width", 1.5f }, { "warmth", 0.30f }, { "eqHigh", 2.0f } } },
            { "MOON TAPE", 2, { { "sat", 0.30f }, { "tape", 0.65f }, { "chorus", 0.30f }, { "delay", 0.30f }, { "reverb", 0.40f },
                                { "delayTime", 320.f }, { "warmth", 0.50f }, { "air", 0.15f }, { "eqHigh", -2.0f },
                                { "eqLow", 2.0f }, { "width", 1.2f } } },
            { "FAIRY DUST", 4, { { "chorus", 0.70f }, { "trem", 0.35f }, { "tremRate", 6.0f }, { "reverb", 0.60f },
                                 { "reverbSize", 0.85f }, { "air", 0.55f }, { "width", 1.6f }, { "delay", 0.20f }, { "tape", 0.20f },
                                 { "sat", 0.0f } } },
            { "WARM CHIME", 3, { { "sat", 0.40f }, { "tape", 0.35f }, { "chorus", 0.25f }, { "reverb", 0.35f }, { "warmth", 0.55f },
                                 { "air", 0.20f }, { "eqLow", 2.5f }, { "eqMid", 1.5f }, { "eqMidFreq", 900.f },
                                 { "width", 1.1f }, { "delay", 0.15f } } },
            { "DEEP FOREST", 5, { { "sat", 0.20f }, { "tape", 0.50f }, { "chorus", 0.45f }, { "delay", 0.45f }, { "delayFb", 0.45f },
                                  { "delayTime", 360.f }, { "reverb", 0.65f }, { "reverbSize", 0.90f }, { "warmth", 0.45f },
                                  { "air", 0.10f }, { "eqHigh", -3.0f }, { "width", 1.4f }, { "glide", 0.20f } } },
            { "PIXIE POP", 2, { { "sat", 0.45f }, { "chorus", 0.40f }, { "trem", 0.40f }, { "tremRate", 7.5f }, { "delay", 0.30f },
                                { "delayTime", 140.f }, { "reverb", 0.25f }, { "air", 0.50f }, { "width", 1.3f },
                                { "eqMid", 2.0f }, { "eqMidFreq", 2500.f }, { "vibrato", 0.15f } } },
        };
        return list;
    }
}

//==============================================================================
EnchantrProcessor::EnchantrProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    formatManager.registerBasicFormats();
    rng.setSeedRandomly();

    for (int i = 0; i < 16; ++i)
        synth.addVoice (new EnchantrVoice (apvts, glideShared));

    for (auto& s : scope)
        s.store (0.f);

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pSat = get ("sat");         pTape = get ("tape");       pChorus = get ("chorus");
    pTrem = get ("trem");       pDelay = get ("delay");     pReverb = get ("reverb");
    pTremRate = get ("tremRate"); pDelayTime = get ("delayTime");
    pDelayFb = get ("delayFb"); pRevSize = get ("reverbSize");
    pWarmth = get ("warmth");   pAir = get ("air");         pWidth = get ("width");
    pEqLow = get ("eqLow");     pEqMid = get ("eqMid");     pEqMidFreq = get ("eqMidFreq");
    pEqHigh = get ("eqHigh");   pMix = get ("mix");         pOut = get ("out");
    pAttack = get ("attack");   pRelease = get ("release"); pRoot = get ("root");

    refreshPresets();
}

juce::AudioProcessorValueTreeState::ParameterLayout EnchantrProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using Attr = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addF = [&layout] (const char* id, const char* name, float lo, float hi, float def,
                           float skew = 1.0f, const char* unit = "")
    {
        layout.add (std::make_unique<F> (juce::ParameterID { id, 1 }, name,
                                         juce::NormalisableRange<float> (lo, hi, 0.001f, skew), def,
                                         Attr().withLabel (unit)));
    };

    // the six "pedals" Level Up rolls
    addF ("sat",    "Saturation", 0.f, 1.f, 0.30f);
    addF ("tape",   "Tape",       0.f, 1.f, 0.25f);
    addF ("chorus", "Chorus",     0.f, 1.f, 0.30f);
    addF ("trem",   "Tremolo",    0.f, 1.f, 0.00f);
    addF ("delay",  "Echo",       0.f, 1.f, 0.25f);
    addF ("reverb", "Reverb",     0.f, 1.f, 0.30f);

    // pedal details (rolled by Level Up too)
    addF ("tremRate",   "Tremolo Rate", 1.f, 9.f, 4.5f, 1.f, "Hz");
    addF ("delayTime",  "Echo Time",    60.f, 500.f, 240.f, 1.f, "ms");
    addF ("delayFb",    "Echo Feedback", 0.f, 0.7f, 0.3f);
    addF ("reverbSize", "Reverb Size",  0.2f, 0.95f, 0.6f);

    // tweak tab
    addF ("warmth",    "Warmth",   0.f, 1.f, 0.35f);
    addF ("air",       "Air",      0.f, 1.f, 0.30f);
    addF ("width",     "Width",    0.f, 2.f, 1.2f);
    addF ("eqLow",     "EQ Low",   -12.f, 12.f, 0.f, 1.f, "dB");
    addF ("eqMid",     "EQ Mid",   -12.f, 12.f, 0.f, 1.f, "dB");
    addF ("eqMidFreq", "Mid Freq", 200.f, 5000.f, 1200.f, 0.4f, "Hz");
    addF ("eqHigh",    "EQ High",  -12.f, 12.f, 0.f, 1.f, "dB");
    addF ("attack",    "Attack",   0.001f, 1.f, 0.005f, 0.3f, "s");
    addF ("release",   "Release",  0.01f, 3.f, 0.4f, 0.4f, "s");

    // main screen
    addF ("mix",     "Mix",     0.f, 1.f, 0.9f);
    addF ("out",     "Output",  -12.f, 12.f, 0.f, 1.f, "dB");
    addF ("glide",   "Glide",   0.f, 1.f, 0.f);
    addF ("vibrato", "Vibrato", 0.f, 1.f, 0.f);

    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "root", 1 }, "Root Note", 24, 96, 60));

    return layout;
}

//==============================================================================
void EnchantrProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    currentSampleRate = sr;
    synth.setCurrentPlaybackSampleRate (sr);

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 2 };

    tapeDelay.setMaximumDelayInSamples ((int) (0.02 * sr) + 4);
    tapeDelay.prepare (spec);
    echoDelay.setMaximumDelayInSamples ((int) (0.6 * sr) + 4);
    echoDelay.prepare (spec);

    chorus.prepare (spec);
    chorus.setRate (0.9f);
    chorus.setDepth (0.35f);
    chorus.setCentreDelay (8.0f);
    chorus.setFeedback (0.0f);

    reverb.prepare (spec);

    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 5000.0f, 0.707f);
    for (auto& f : airHp)
    {
        f.prepare (spec);
        f.coefficients = hp;
        f.reset();
    }
    for (int i = 0; i < 2; ++i)
    {
        eqLowF[i].prepare (spec);
        eqMidF[i].prepare (spec);
        eqHighF[i].prepare (spec);
    }
    updateEq (true);

    const double ramp = 0.05;
    smSat.reset (sr, ramp);   smTape.reset (sr, ramp);  smTrem.reset (sr, ramp);
    smDelay.reset (sr, ramp); smDelayTime.reset (sr, 0.15);
    smMix.reset (sr, ramp);   smOut.reset (sr, ramp);

    smSat.setCurrentAndTargetValue (pSat->load());
    smTape.setCurrentAndTargetValue (pTape->load());
    smTrem.setCurrentAndTargetValue (pTrem->load());
    smDelay.setCurrentAndTargetValue (pDelay->load());
    smDelayTime.setCurrentAndTargetValue (pDelayTime->load() * 0.001f * (float) sr);
    smMix.setCurrentAndTargetValue (pMix->load());
    smOut.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));

    dryBuf.setSize (2, samplesPerBlock);
}

bool EnchantrProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EnchantrProcessor::updateEq (bool force)
{
    const float lo = pEqLow->load(), mid = pEqMid->load(), mf = pEqMidFreq->load(), hi = pEqHigh->load();

    if (! force && lo == lastLo && mid == lastMid && mf == lastMidF && hi == lastHi)
        return;

    lastLo = lo; lastMid = mid; lastMidF = mf; lastHi = hi;

    auto cl = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (currentSampleRate, 160.0f, 0.707f, juce::Decibels::decibelsToGain (lo));
    auto cm = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, mf, 0.8f, juce::Decibels::decibelsToGain (mid));
    auto ch = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 6000.0f, 0.707f, juce::Decibels::decibelsToGain (hi));

    for (int i = 0; i < 2; ++i)
    {
        eqLowF[i].coefficients = cl;
        eqMidF[i].coefficients = cm;
        eqHighF[i].coefficients = ch;
    }
}

//==============================================================================
void EnchantrProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const float sr = (float) currentSampleRate;

    // preview note (when a sample loads or Level Up is pressed)
    if (previewPending.exchange (false) && sampleLoaded.load())
    {
        previewNote = (int) pRoot->load();
        midi.addEvent (juce::MidiMessage::noteOn (1, previewNote, (juce::uint8) 100), 0);
        previewSamplesLeft = (int) (sr * 1.5f);
    }
    if (previewSamplesLeft > 0)
    {
        if (previewSamplesLeft <= n)
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, previewNote), juce::jmax (0, previewSamplesLeft - 1));
            previewSamplesLeft = 0;
        }
        else
        {
            previewSamplesLeft -= n;
        }
    }

    // glide: notes in this block glide from the last note of earlier blocks
    {
        const float newest = glideShared.newest.load();
        if (newest >= 0.0f)
            glideShared.prev.store (newest);
    }

    buffer.clear();
    synth.renderNextBlock (buffer, midi, 0, n);

    if (buffer.getNumChannels() < 2)
        return;

    dryBuf.setSize (2, n, false, false, true);
    dryBuf.copyFrom (0, 0, buffer, 0, 0, n);
    dryBuf.copyFrom (1, 0, buffer, 1, 0, n);

    // targets
    smSat.setTargetValue (pSat->load());
    smTape.setTargetValue (pTape->load());
    smTrem.setTargetValue (pTrem->load());
    smDelay.setTargetValue (pDelay->load());
    smDelayTime.setTargetValue (pDelayTime->load() * 0.001f * sr);
    smMix.setTargetValue (pMix->load());
    smOut.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));

    const float warm = pWarmth->load();
    const float air = pAir->load();
    const float width = pWidth->load();
    const float tapeCut = 15000.0f - pTape->load() * 9000.0f;
    const float tapeA = 1.0f - std::exp (-twoPi * tapeCut / sr);
    const float tremRate = pTremRate->load();
    const float fb = pDelayFb->load();

    float* data[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    // --- stage 1: saturation + tape wobble ---------------------------------
    for (int i = 0; i < n; ++i)
    {
        const float s = smSat.getNextValue();
        const float tp = smTape.getNextValue();
        const float pre = 1.0f + s * 5.0f + warm * 2.0f;
        const float wetAmt = juce::jlimit (0.f, 1.f, s * 1.2f + warm * 0.5f);
        const float bias = 0.12f * warm;
        const float biasTanh = std::tanh (pre * bias);

        tapePhase1 += twoPi * 0.55f / sr;  if (tapePhase1 >= twoPi) tapePhase1 -= twoPi;
        tapePhase2 += twoPi * 6.3f / sr;   if (tapePhase2 >= twoPi) tapePhase2 -= twoPi;
        const float mod = std::sin (tapePhase1) + 0.25f * std::sin (tapePhase2);
        const float dSamp = (0.0045f + tp * 0.0035f * mod) * sr;

        for (int ch = 0; ch < 2; ++ch)
        {
            float x = data[ch][i];

            const float y = (std::tanh (pre * (x + bias)) - biasTanh) / pre * (1.0f + 0.3f * s);
            x += wetAmt * (y - x);

            tapeDelay.pushSample (ch, x);
            float w = tapeDelay.popSample (ch, dSamp);
            tapeLp[ch] += tapeA * (w - tapeLp[ch]);
            w += tp * (tapeLp[ch] - w);
            x += tp * (w - x);

            data[ch][i] = x;
        }
    }

    // --- stage 2: chorus ----------------------------------------------------
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    chorus.setMix (pChorus->load());
    chorus.process (ctx);

    // --- stage 3: tremolo + echo -------------------------------------------
    for (int i = 0; i < n; ++i)
    {
        const float td = smTrem.getNextValue();
        const float dm = smDelay.getNextValue();
        const float dt = juce::jlimit (2.0f, (float) (0.55 * currentSampleRate), smDelayTime.getNextValue());

        tremPhase += twoPi * tremRate / sr;
        if (tremPhase >= twoPi) tremPhase -= twoPi;

        float x[2];
        x[0] = data[0][i] * (1.0f - td * 0.5f * (1.0f + std::sin (tremPhase)));
        x[1] = data[1][i] * (1.0f - td * 0.5f * (1.0f + std::sin (tremPhase + 1.5708f)));

        const float dL = echoDelay.popSample (0, dt);
        const float dR = echoDelay.popSample (1, dt);
        echoLp[0] += 0.35f * (dL - echoLp[0]);
        echoLp[1] += 0.35f * (dR - echoLp[1]);

        const float mono = 0.5f * (x[0] + x[1]);
        echoDelay.pushSample (0, mono + fb * echoLp[1]);
        echoDelay.pushSample (1, fb * echoLp[0]);

        data[0][i] = x[0] + dm * dL;
        data[1][i] = x[1] + dm * dR;
    }

    // --- stage 4: reverb ----------------------------------------------------
    juce::dsp::Reverb::Parameters rp;
    rp.roomSize = pRevSize->load();
    rp.damping = 0.55f;
    rp.wetLevel = pReverb->load() * 0.25f;
    rp.dryLevel = 0.5f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
    reverb.process (ctx);

    // --- stage 5: air, EQ, width, mix, output ------------------------------
    updateEq (false);

    for (int i = 0; i < n; ++i)
    {
        const float mix = smMix.getNextValue();
        const float outG = smOut.getNextValue();

        float w[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            float x = data[ch][i];

            const float highs = airHp[ch].processSample (x);
            x += air * 0.5f * std::tanh (highs * 4.0f);

            x = eqLowF[ch].processSample (x);
            x = eqMidF[ch].processSample (x);
            x = eqHighF[ch].processSample (x);

            w[ch] = x;
        }

        const float mid = 0.5f * (w[0] + w[1]);
        const float side = 0.5f * (w[0] - w[1]) * width;
        w[0] = mid + side;
        w[1] = mid - side;

        for (int ch = 0; ch < 2; ++ch)
        {
            const float dry = dryBuf.getSample (ch, i);
            data[ch][i] = softClip ((dry + mix * (w[ch] - dry)) * outG);
        }

        if (++scopeCounter >= 64)
        {
            scopeCounter = 0;
            const int head = scopeHead.load();
            scope[(size_t) head].store (0.5f * (data[0][i] + data[1][i]));
            scopeHead.store ((head + 1) % scopeSize);
        }
    }
}

//==============================================================================
bool EnchantrProcessor::loadSample (const juce::File& file, bool preview)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    const int len = (int) juce::jmin ((juce::int64) reader->lengthInSamples,
                                      (juce::int64) (reader->sampleRate * 20.0));
    const int chans = juce::jmin ((int) reader->numChannels, 2);

    juce::AudioBuffer<float> buf (chans, len);
    reader->read (&buf, 0, len, 0, true, true);

    const float peak = buf.getMagnitude (0, len);
    if (peak > 0.0001f)
        buf.applyGain (0.8f / peak);

    synth.clearSounds();
    synth.addSound (new EnchantrSound (std::move (buf), reader->sampleRate));

    sampleName = file.getFileNameWithoutExtension();
    sampleLoaded = true;
    apvts.state.setProperty ("samplePath", file.getFullPathName(), nullptr);

    if (preview)
        previewPending = true;

    return true;
}

void EnchantrProcessor::setParamReal (const char* id, float v)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void EnchantrProcessor::setLevel (int newLevel)
{
    level = newLevel;
    apvts.state.setProperty ("level", newLevel, nullptr);
}

void EnchantrProcessor::levelUp()
{
    auto rnd = [this] (float lo, float hi) { return lo + (hi - lo) * rng.nextFloat(); };

    // gentle ceilings so a roll never gets harsh
    const float maxAmt[6] = { 0.6f, 0.7f, 0.8f, 0.5f, 0.5f, 0.7f };

    auto roll = [&]
    {
        std::array<float, 17> v {};
        bool on[6];
        int count = 0;
        for (int i = 0; i < 6; ++i)
        {
            on[i] = rng.nextFloat() < 0.55f;
            count += on[i] ? 1 : 0;
        }
        while (count < 3)
        {
            const int k = rng.nextInt (6);
            if (! on[k]) { on[k] = true; ++count; }
        }
        for (int i = 0; i < 6; ++i)
            v[(size_t) i] = on[i] ? rnd (0.25f, 1.0f) * maxAmt[i] : 0.0f;

        v[6]  = rnd (2.0f, 7.0f);       // tremRate
        v[7]  = rnd (120.0f, 380.0f);   // delayTime
        v[8]  = rnd (0.15f, 0.5f);      // delayFb
        v[9]  = rnd (0.35f, 0.9f);      // reverbSize
        v[10] = rnd (0.2f, 0.55f);      // warmth
        v[11] = rnd (0.15f, 0.55f);     // air
        v[12] = rnd (1.0f, 1.6f);       // width
        v[13] = rnd (-2.0f, 3.0f);      // eqLow
        v[14] = rnd (-3.0f, 3.0f);      // eqMid
        v[15] = rnd (400.0f, 3500.0f);  // eqMidFreq
        v[16] = rnd (-2.0f, 3.0f);      // eqHigh
        return v;
    };

    auto normalised = [&] (const std::array<float, 17>& real)
    {
        std::array<float, 17> nrm {};
        for (size_t i = 0; i < 17; ++i)
            if (auto* p = apvts.getParameter (rolledIds[i]))
                nrm[i] = p->convertTo0to1 (real[i]);
        return nrm;
    };

    auto distance = [&] (const std::array<float, 17>& a)
    {
        float best = 10.0f;
        for (const auto& h : rollHistory)
        {
            float acc = 0.0f;
            for (size_t i = 0; i < 17; ++i)
                acc += (a[i] - h[i]) * (a[i] - h[i]);
            best = juce::jmin (best, std::sqrt (acc / 17.0f));
        }
        return best;
    };

    // never repeat (or nearly repeat) one of the recent rolls
    std::array<float, 17> chosen = roll();
    float chosenDist = distance (normalised (chosen));
    for (int attempt = 0; attempt < 30 && chosenDist < 0.2f; ++attempt)
    {
        const auto cand = roll();
        const float dist = distance (normalised (cand));
        if (dist > chosenDist)
        {
            chosen = cand;
            chosenDist = dist;
        }
    }

    rollHistory.push_back (normalised (chosen));
    if (rollHistory.size() > 12)
        rollHistory.erase (rollHistory.begin());

    for (size_t i = 0; i < 17; ++i)
        setParamReal (rolledIds[i], chosen[i]);

    setLevel (level.load() + 1);
    apvts.state.setProperty ("presetName", "", nullptr);

    previewPending = true;
}

//==============================================================================
juce::File EnchantrProcessor::getPresetFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Enchantr").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

void EnchantrProcessor::refreshPresets()
{
    presets.clear();

    for (const auto& f : factoryPresets())
    {
        PresetInfo info;
        info.name = f.name;
        info.factory = true;
        info.level = f.level;
        presets.push_back (info);
    }

    auto files = getPresetFolder().findChildFiles (juce::File::findFiles, false, "*.enchantr");
    files.sort();
    for (const auto& file : files)
    {
        if (auto xml = juce::XmlDocument::parse (file))
        {
            if (! xml->hasTagName ("ENCHANTR_PRESET"))
                continue;
            PresetInfo info;
            info.name = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
            info.factory = false;
            info.level = xml->getIntAttribute ("level", 1);
            info.file = file;
            presets.push_back (info);
        }
    }
}

void EnchantrProcessor::resetToDefaults (bool keepRoot)
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (keepRoot && rp->paramID == "root")
                continue;
            rp->setValueNotifyingHost (rp->getDefaultValue());
        }
}

bool EnchantrProcessor::loadPreset (int index)
{
    if (index < 0 || index >= (int) presets.size())
        return false;

    const auto& info = presets[(size_t) index];

    if (info.factory)
    {
        const auto& list = factoryPresets();
        const int fi = index; // factory presets come first, in order
        if (fi >= (int) list.size())
            return false;

        resetToDefaults (true);
        for (const auto& kv : list[(size_t) fi].v)
            setParamReal (kv.first, kv.second);
        setLevel (list[(size_t) fi].level);
        apvts.state.setProperty ("presetName", info.name, nullptr);
        previewPending = true;
        return true;
    }

    auto xml = juce::XmlDocument::parse (info.file);
    if (xml == nullptr || ! xml->hasTagName ("ENCHANTR_PRESET"))
        return false;

    resetToDefaults (false);
    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        setParamReal (e->getStringAttribute ("id").toRawUTF8(), (float) e->getDoubleAttribute ("value"));

    setLevel (xml->getIntAttribute ("level", 1));
    apvts.state.setProperty ("presetName", info.name, nullptr);

    const juce::File sample (xml->getStringAttribute ("samplePath"));
    if (sample.existsAsFile())
        loadSample (sample, false);

    previewPending = true;
    return true;
}

bool EnchantrProcessor::saveUserPreset (const juce::String& rawName)
{
    const auto name = rawName.trim().substring (0, 24).toUpperCase();
    if (name.isEmpty())
        return false;

    juce::XmlElement xml ("ENCHANTR_PRESET");
    xml.setAttribute ("name", name);
    xml.setAttribute ("level", level.load());
    xml.setAttribute ("samplePath", apvts.state.getProperty ("samplePath").toString());

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", rp->paramID);
            e->setAttribute ("value", (double) rp->convertFrom0to1 (rp->getValue()));
        }

    const auto file = getPresetFolder().getChildFile (juce::File::createLegalFileName (name) + ".enchantr");
    if (! xml.writeTo (file))
        return false;

    apvts.state.setProperty ("presetName", name, nullptr);
    refreshPresets();
    return true;
}

bool EnchantrProcessor::deleteUserPreset (int index)
{
    if (index < 0 || index >= (int) presets.size() || presets[(size_t) index].factory)
        return false;

    const bool ok = presets[(size_t) index].file.deleteFile();
    if (ok)
    {
        if (getPresetName() == presets[(size_t) index].name)
            apvts.state.setProperty ("presetName", "", nullptr);
        refreshPresets();
    }
    return ok;
}

juce::String EnchantrProcessor::getPresetName() const
{
    return apvts.state.getProperty ("presetName").toString();
}

int EnchantrProcessor::getCurrentPresetIndex() const
{
    const auto name = getPresetName();
    if (name.isEmpty())
        return -1;
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == name)
            return (int) i;
    return -1;
}

//==============================================================================
juce::AudioProcessorEditor* EnchantrProcessor::createEditor()
{
    return new EnchantrEditor (*this);
}

void EnchantrProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void EnchantrProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            level = (int) apvts.state.getProperty ("level", 0);

            const juce::String path = apvts.state.getProperty ("samplePath").toString();
            if (path.isNotEmpty())
            {
                juce::File f (path);
                if (f.existsAsFile())
                    loadSample (f, false);
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EnchantrProcessor();
}
