#include "Engine/VoicePool.h"
#include <juce_core/juce_core.h>

using namespace dsl;

namespace
{
    std::shared_ptr<const juce::AudioBuffer<float>> makeConstantBuffer(int numChannels, int numSamples, float value)
    {
        auto buffer = std::make_shared<juce::AudioBuffer<float>>(numChannels, numSamples);
        for (int c = 0; c < numChannels; ++c)
            for (int i = 0; i < numSamples; ++i)
                buffer->setSample(c, i, value);
        return buffer;
    }
}

class VoicePoolTests : public juce::UnitTest
{
public:
    VoicePoolTests() : juce::UnitTest("VoicePool", "Engine") {}

    void runTest() override
    {
        beginTest("A new hit layers on top of a still-ringing previous hit (let it ring)");
        {
            VoicePool pool(8);
            pool.prepare(44100.0);

            auto sample = makeConstantBuffer(1, 1000, 1.0f); // long enough to stay active through this test

            juce::AudioBuffer<float> busBuffer(1, 4);
            std::vector<juce::AudioBuffer<float>*> buses { &busBuffer };

            pool.startVoice(sample, 44100.0, 1.0f, 0);
            busBuffer.clear();
            pool.renderNextBlock(buses, 4); // advances voice 1 to sample index 4

            pool.startVoice(sample, 44100.0, 1.0f, 0); // second hit starts while the first is still ringing
            busBuffer.clear();
            pool.renderNextBlock(buses, 4);

            // Both voices are still within the sample and neither has finished,
            // so every output sample must be the sum of both (1.0 + 1.0).
            for (int i = 0; i < busBuffer.getNumSamples(); ++i)
                expectWithinAbsoluteError(busBuffer.getSample(0, i), 2.0f, 0.0001f);

            expectEquals(pool.getNumActiveVoices(), 2);
        }

        beginTest("A voice becomes inactive once it runs past the end of its sample");
        {
            VoicePool pool(4);
            pool.prepare(44100.0);

            auto shortSample = makeConstantBuffer(1, 4, 1.0f);
            juce::AudioBuffer<float> busBuffer(1, 10);
            std::vector<juce::AudioBuffer<float>*> buses { &busBuffer };

            pool.startVoice(shortSample, 44100.0, 1.0f, 0);
            busBuffer.clear();
            pool.renderNextBlock(buses, 10); // block is longer than the 4-sample source

            expectEquals(pool.getNumActiveVoices(), 0);
        }

        beginTest("Per-channel fader gain is applied to the rendered output");
        {
            VoicePool pool(4);
            pool.prepare(44100.0);

            auto sample = makeConstantBuffer(1, 100, 1.0f);
            juce::AudioBuffer<float> busBuffer(1, 4);
            std::vector<juce::AudioBuffer<float>*> buses { &busBuffer };

            pool.startVoice(sample, 44100.0, 0.5f, 0);
            busBuffer.clear();
            pool.renderNextBlock(buses, 4);

            for (int i = 0; i < busBuffer.getNumSamples(); ++i)
                expectWithinAbsoluteError(busBuffer.getSample(0, i), 0.5f, 0.0001f);
        }

        beginTest("Voices routed to different buses never bleed into each other");
        {
            VoicePool pool(4);
            pool.prepare(44100.0);

            auto sample = makeConstantBuffer(1, 100, 1.0f);
            juce::AudioBuffer<float> busA(1, 4);
            juce::AudioBuffer<float> busB(1, 4);
            std::vector<juce::AudioBuffer<float>*> buses { &busA, &busB };

            pool.startVoice(sample, 44100.0, 1.0f, 0); // -> busA only
            busA.clear();
            busB.clear();
            pool.renderNextBlock(buses, 4);

            for (int i = 0; i < 4; ++i)
            {
                expectWithinAbsoluteError(busA.getSample(0, i), 1.0f, 0.0001f);
                expectWithinAbsoluteError(busB.getSample(0, i), 0.0f, 0.0001f);
            }
        }

        beginTest("Mono overhead and room samples obey hard left and hard right pan");
        {
            VoicePool pool(4);
            pool.prepare(44100.0);

            auto sample = makeConstantBuffer(1, 100, 1.0f);
            juce::AudioBuffer<float> stereoBus(2, 4);
            std::vector<juce::AudioBuffer<float>*> buses { &stereoBus };

            pool.startVoice(sample, 44100.0, 1.0f, 0, 1.0, -1.0f);
            stereoBus.clear();
            pool.renderNextBlock(buses, 4);
            for (int i = 0; i < 4; ++i)
            {
                expectWithinAbsoluteError(stereoBus.getSample(0, i), 1.0f, 0.0001f);
                expectWithinAbsoluteError(stereoBus.getSample(1, i), 0.0f, 0.0001f);
            }

            pool.reset();
            pool.startVoice(sample, 44100.0, 1.0f, 0, 1.0, 1.0f);
            stereoBus.clear();
            pool.renderNextBlock(buses, 4);
            for (int i = 0; i < 4; ++i)
            {
                expectWithinAbsoluteError(stereoBus.getSample(0, i), 0.0f, 0.0001f);
                expectWithinAbsoluteError(stereoBus.getSample(1, i), 1.0f, 0.0001f);
            }
        }

        beginTest("A stereo ambience sample preserves its left and right channels");
        {
            VoicePool pool(2);
            pool.prepare(44100.0);

            auto sample = std::make_shared<juce::AudioBuffer<float>>(2, 100);
            sample->clear();
            sample->applyGain(0.0f);
            for (int i = 0; i < sample->getNumSamples(); ++i)
            {
                sample->setSample(0, i, 0.25f);
                sample->setSample(1, i, 0.75f);
            }

            juce::AudioBuffer<float> stereoBus(2, 4);
            std::vector<juce::AudioBuffer<float>*> buses { &stereoBus };
            pool.startVoice(sample, 44100.0, 1.0f, 0);
            stereoBus.clear();
            pool.renderNextBlock(buses, 4);

            for (int i = 0; i < 4; ++i)
            {
                expectWithinAbsoluteError(stereoBus.getSample(0, i), 0.25f, 0.0001f);
                expectWithinAbsoluteError(stereoBus.getSample(1, i), 0.75f, 0.0001f);
            }
        }

        beginTest("Choke releases only voices belonging to the requested instrument");
        {
            VoicePool pool(4);
            pool.prepare(1000.0); // 2 ms release = two rendered samples

            auto sample = makeConstantBuffer(1, 100, 1.0f);
            juce::AudioBuffer<float> busBuffer(1, 4);
            std::vector<juce::AudioBuffer<float>*> buses { &busBuffer };

            pool.startVoice(sample, 1000.0, 1.0f, 0, 1.0, 0.0f, 101);
            pool.startVoice(sample, 1000.0, 1.0f, 0, 1.0, 0.0f, 202);
            pool.chokeVoices(101, 2.0);
            busBuffer.clear();
            pool.renderNextBlock(buses, 4);

            expectEquals(pool.getNumActiveVoices(), 1);
            // The unrelated group continues at unity after the choked voice
            // has completed its short click-free release.
            expectWithinAbsoluteError(busBuffer.getSample(0, 3), 1.0f, 0.0001f);
        }

        beginTest("A channel's meter peak decays back to silence once its voice finishes ringing");
        {
            VoicePool pool(4);
            pool.prepare(44100.0);

            auto shortSample = makeConstantBuffer(1, 4, 1.0f);
            juce::AudioBuffer<float> busBuffer(1, 4);
            std::vector<juce::AudioBuffer<float>*> buses { &busBuffer };

            constexpr uint64_t meterKey = 5;
            pool.startVoice(shortSample, 44100.0, 1.0f, 0, 1.0, 0.0f, 0, meterKey);
            busBuffer.clear();
            pool.renderNextBlock(buses, 4); // the 4-sample voice fully rings out within this block

            expectEquals(pool.getNumActiveVoices(), 0);
            expectGreaterThan(pool.getChannelPeak(meterKey), 0.9f);

            // No voices left at all, so only the blanket per-block decay is
            // touching this bucket now — it must still fade towards silence
            // rather than sticking at its last value forever.
            for (int i = 0; i < 80; ++i)
            {
                busBuffer.clear();
                pool.renderNextBlock(buses, 4);
            }
            expectLessThan(pool.getChannelPeak(meterKey), 0.001f);
        }
    }
};

static VoicePoolTests voicePoolTestsInstance;
