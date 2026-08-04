#include "VoicePool.h"
#include <juce_core/juce_core.h>

namespace dsl
{

namespace
{
    float readInterpolated(const juce::AudioBuffer<float>& buffer, int channel, double position)
    {
        const int i0 = (int) position;
        const int i1 = juce::jmin(i0 + 1, buffer.getNumSamples() - 1);
        const float frac = (float) (position - (double) i0);
        const float s0 = buffer.getSample(channel, i0);
        const float s1 = buffer.getSample(channel, i1);
        return s0 + frac * (s1 - s0);
    }

    // Maps a destination bus channel to a source sample channel when their
    // channel counts differ: mono spot samples duplicate to every bus
    // channel; a stereo sample feeding a mono bus is downmixed by the caller.
    int mapSourceChannel(int destChannel, int numSourceChannels)
    {
        if (numSourceChannels == 1)
            return 0;

        return juce::jmin(destChannel, numSourceChannels - 1);
    }
}

VoicePool::VoicePool(int maxVoices)
{
    voices.resize((size_t) juce::jmax(1, maxVoices));
}

void VoicePool::prepare(double engineSampleRateToUse)
{
    engineSampleRate = engineSampleRateToUse > 0.0 ? engineSampleRateToUse : 44100.0;
    reset();
}

Voice* VoicePool::findFreeOrStealVoice()
{
    for (auto& v : voices)
        if (! v.active)
            return &v;

    Voice* oldest = &voices[0];
    for (auto& v : voices)
        if (v.startOrder < oldest->startOrder)
            oldest = &v;

    return oldest;
}

void VoicePool::startVoice(std::shared_ptr<const juce::AudioBuffer<float>> buffer,
                            double sourceSampleRate, float gainLinear, int busIndex,
                            double extraPitchRatio, float pan, uint64_t voiceGroup)
{
    if (buffer == nullptr || buffer->getNumSamples() == 0)
        return;

    auto* v = findFreeOrStealVoice();
    v->buffer = std::move(buffer);
    v->readPosition = 0.0;
    v->playbackRatio = (sourceSampleRate > 0.0 ? sourceSampleRate / engineSampleRate : 1.0) * extraPitchRatio;
    v->gainLinear = gainLinear;
    v->pan = juce::jlimit(-1.0f, 1.0f, pan);
    v->busIndex = busIndex;
    v->voiceGroup = voiceGroup;
    v->chokeGain = 1.0f;
    v->chokeStepPerSample = 0.0f;
    v->active = true;
    v->startOrder = ++voiceCounter;
}

void VoicePool::chokeVoices(uint64_t voiceGroup, double releaseMs)
{
    const float releaseSamples = (float) juce::jmax(1.0, engineSampleRate * releaseMs / 1000.0);
    for (auto& v : voices)
        if (v.active && v.voiceGroup == voiceGroup)
            v.chokeStepPerSample = 1.0f / releaseSamples;
}

void VoicePool::renderNextBlock(const std::vector<juce::AudioBuffer<float>*>& busBuffers, int numSamples)
{
    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        if (v.busIndex < 0 || v.busIndex >= (int) busBuffers.size() || busBuffers[(size_t) v.busIndex] == nullptr)
        {
            v.active = false;
            continue;
        }

        auto& busBuffer = *busBuffers[(size_t) v.busIndex];
        const int numSourceChannels = v.buffer->getNumChannels();
        const int numBusChannels = busBuffer.getNumChannels();
        const int numSourceSamples = v.buffer->getNumSamples();
        const bool downmixToMono = numSourceChannels >= 2 && numBusChannels == 1;

        for (int i = 0; i < numSamples; ++i)
        {
            if (v.readPosition >= (double) (numSourceSamples - 1))
            {
                v.active = false;
                break;
            }

            for (int c = 0; c < numBusChannels; ++c)
            {
                float sample;
                if (downmixToMono)
                {
                    sample = 0.5f * (readInterpolated(*v.buffer, 0, v.readPosition)
                                      + readInterpolated(*v.buffer, 1, v.readPosition));
                }
                else
                {
                    sample = readInterpolated(*v.buffer, mapSourceChannel(c, numSourceChannels), v.readPosition);
                }

                float panGain = 1.0f;
                if (numSourceChannels == 1 && numBusChannels >= 2)
                {
                    if (c == 0)
                        panGain = v.pan > 0.0f ? 1.0f - v.pan : 1.0f;
                    else if (c == 1)
                        panGain = v.pan < 0.0f ? 1.0f + v.pan : 1.0f;
                }

                busBuffer.addSample(c, i, sample * v.gainLinear * v.chokeGain * panGain);
            }

            v.readPosition += v.playbackRatio;
            if (v.chokeStepPerSample > 0.0f)
            {
                v.chokeGain = juce::jmax(0.0f, v.chokeGain - v.chokeStepPerSample);
                if (v.chokeGain <= 0.0f)
                {
                    v.active = false;
                    break;
                }
            }
        }
    }
}

void VoicePool::reset()
{
    for (auto& v : voices)
    {
        v.active = false;
        v.buffer = nullptr;
        v.voiceGroup = 0;
    }
}

int VoicePool::getNumActiveVoices() const
{
    int count = 0;
    for (auto& v : voices)
        if (v.active)
            ++count;

    return count;
}

} // namespace dsl
