#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <memory>

namespace dsl
{

/** Loads sample files into RAM once and hands out shared, read-only buffers.
    Must only be called from a non-realtime thread (message thread or a
    background loader thread) — it performs file IO. See docs/01-architecture.md
    section 1.1 for why samples are fully preloaded rather than streamed.
*/
class SamplePool
{
public:
    explicit SamplePool(juce::AudioFormatManager& formatManagerToUse);

    /** Loads absolutePath if not already cached, returns the shared buffer.
        Returns nullptr (and leaves outSourceSampleRate untouched) if the file
        could not be read.
    */
    std::shared_ptr<const juce::AudioBuffer<float>> loadOrGet(const juce::String& absolutePath,
                                                                double& outSourceSampleRate);

    void clear();

    int getNumLoadedSamples() const { return (int) cache.size(); }

private:
    struct CacheEntry
    {
        std::shared_ptr<const juce::AudioBuffer<float>> buffer;
        double sampleRate = 44100.0;
    };

    juce::AudioFormatManager& formatManager;
    std::map<juce::String, CacheEntry> cache;
};

} // namespace dsl
