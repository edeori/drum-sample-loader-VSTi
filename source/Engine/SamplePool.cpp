#include "SamplePool.h"

namespace dsl
{

SamplePool::SamplePool(juce::AudioFormatManager& formatManagerToUse)
    : formatManager(formatManagerToUse)
{
}

std::shared_ptr<const juce::AudioBuffer<float>> SamplePool::loadOrGet(const juce::String& absolutePath,
                                                                        double& outSourceSampleRate)
{
    if (auto it = cache.find(absolutePath); it != cache.end())
    {
        outSourceSampleRate = it->second.sampleRate;
        return it->second.buffer;
    }

    juce::File file(absolutePath);
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
        return nullptr;

    auto buffer = std::make_shared<juce::AudioBuffer<float>>((int) reader->numChannels,
                                                               (int) reader->lengthInSamples);
    reader->read(buffer.get(), 0, (int) reader->lengthInSamples, 0, true, true);

    CacheEntry entry { buffer, reader->sampleRate };
    cache[absolutePath] = entry;

    outSourceSampleRate = entry.sampleRate;
    return buffer;
}

void SamplePool::clear()
{
    cache.clear();
}

} // namespace dsl
