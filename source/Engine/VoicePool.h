#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>
#include <vector>

namespace dsl
{

/** One playing instance of one mic-channel sample. A single hit spawns one
    Voice per mic channel in its round-robin slot (docs/03-editor-ui.md,
    section 3.3), all started in the same audio block for sample-accurate sync.
*/
struct Voice
{
    std::shared_ptr<const juce::AudioBuffer<float>> buffer;
    double readPosition = 0.0;   // fractional index into buffer, for resampling
    double playbackRatio = 1.0;  // sourceSampleRate / engineSampleRate
    float gainLinear = 1.0f;
    float pan = 0.0f;
    int busIndex = -1;
    uint64_t voiceGroup = 0;
    uint64_t meterKey = 0;
    bool swapStereo = false;
    float chokeGain = 1.0f;
    float chokeStepPerSample = 0.0f;
    bool active = false;
    uint64_t startOrder = 0;
};

/** Fixed-size, statically-allocated voice pool — no allocation on the audio
    thread. Every hit's voices play to completion independently ("let it ring",
    docs/03-editor-ui.md section 3.5): a new hit never stops a previous one.
    Stealing only happens as an overflow safety net if every voice is busy
    (docs/01-architecture.md section 1.3), which is not expected in normal use.
*/
class VoicePool
{
public:
    explicit VoicePool(int maxVoices);

    void prepare(double engineSampleRateToUse);

    void startVoice(std::shared_ptr<const juce::AudioBuffer<float>> buffer,
                     double sourceSampleRate, float gainLinear, int busIndex,
                     double extraPitchRatio = 1.0, float pan = 0.0f,
                     uint64_t voiceGroup = 0, uint64_t meterKey = 0,
                     bool swapStereo = false);

    /** Applies a short click-free release to every active voice belonging to
        one instrument. Used by explicit Crash Choke articulations. */
    void chokeVoices(uint64_t voiceGroup, double releaseMs = 8.0);

    /** Renders and mixes (adds into) every active voice's samples into
        busBuffers[voice.busIndex]. busBuffers must be indexable up to the
        highest busIndex passed to startVoice(); out-of-range indices are
        skipped defensively.
    */
    void renderNextBlock(const std::vector<juce::AudioBuffer<float>*>& busBuffers, int numSamples);

    void reset();

    int getNumActiveVoices() const;

    /** Post-fader, pre-bus-mix decayed peak for one originating mic channel,
        keyed by the same meterKey passed to startVoice() (see
        DrumEngine::meterKeyFor) — lets the Global Mixer show each channel's
        own hits instead of whatever else shares its output bus. Bucketed by
        hash, so two channels can in theory share a bucket; harmless for a
        VU meter (never affects actual audio routing/mixing). */
    float getChannelPeak(uint64_t meterKey) const
    {
        return channelPeaks[meterKey % channelPeaks.size()].load(std::memory_order_relaxed);
    }

private:
    Voice* findFreeOrStealVoice();

    std::vector<Voice> voices;
    double engineSampleRate = 44100.0;
    uint64_t voiceCounter = 0;

    static constexpr size_t numMeterBuckets = 1024;
    std::vector<std::atomic<float>> channelPeaks;
};

} // namespace dsl
