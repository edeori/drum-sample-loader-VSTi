#include "PluginProcessor.h"
#include "Engine/SamplePathRelinker.h"
#include "PluginEditor.h"
#include "Engine/BusRoutingTable.h"
#include "Engine/DefaultKitFactory.h"
#include "Engine/Preset.h"

namespace
{
    std::vector<int> expectedOutputChannelCounts()
    {
        std::vector<int> counts;
        for (auto& bus : dsl::BusRoutingTable::createDefault().buses)
            counts.push_back(bus.numChannels);
        return counts;
    }
}

juce::AudioProcessor::BusesProperties DrumSampleLoaderAudioProcessor::buildBusesProperties()
{
    juce::AudioProcessor::BusesProperties props;

    for (auto& bus : dsl::BusRoutingTable::createDefault().buses)
    {
        auto channelSet = bus.numChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        props = props.withOutput(bus.name, channelSet, true);
    }

    return props;
}

DrumSampleLoaderAudioProcessor::DrumSampleLoaderAudioProcessor()
    : juce::AudioProcessor(buildBusesProperties()),
      busPeaks(dsl::BusRoutingTable::createDefault().buses.size())
{
    formatManager.registerBasicFormats();
    engine.setKit(std::make_shared<dsl::Kit>(dsl::DefaultKitFactory::createDefaultKit()));
    previewMidiCollector.reset(44100.0);
}

void DrumSampleLoaderAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine.prepare(sampleRate, samplesPerBlock);
    previewMidiCollector.reset(sampleRate);
}

juce::AudioProcessorEditor* DrumSampleLoaderAudioProcessor::createEditor()
{
    return new dsl::ui::DrumVaultEditor(*this);
}

void DrumSampleLoaderAudioProcessor::triggerPreviewNote(int midiNote, int velocity)
{
    previewMidiCollector.addMessageToQueue(juce::MidiMessage::noteOn(1, midiNote, (juce::uint8) juce::jlimit(1, 127, velocity)));
}

bool DrumSampleLoaderAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    static const std::vector<int> expected = expectedOutputChannelCounts();

    if (layouts.outputBuses.size() != (int) expected.size())
        return false;

    for (int i = 0; i < layouts.outputBuses.size(); ++i)
    {
        const int numChannels = layouts.outputBuses.getReference(i).size();
        // A host may disable a bus it doesn't want (0 channels); otherwise the
        // channel count must match our fixed mono/stereo layout exactly.
        if (numChannels != 0 && numChannels != expected[(size_t) i])
            return false;
    }

    return true;
}

void DrumSampleLoaderAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numBuses = getBusCount(false);

    std::vector<juce::AudioBuffer<float>> busViews;
    busViews.reserve((size_t) numBuses);
    std::vector<juce::AudioBuffer<float>*> busPointers;
    busPointers.reserve((size_t) numBuses);

    for (int i = 0; i < numBuses; ++i)
    {
        busViews.push_back(getBusBuffer(buffer, false, i));
        busPointers.push_back(&busViews.back());
    }

    juce::MidiBuffer combinedMidi;
    previewMidiCollector.removeNextBlockOfMessages(combinedMidi, buffer.getNumSamples());
    combinedMidi.addEvents(midiMessages, 0, buffer.getNumSamples(), 0);

    engine.processBlock(combinedMidi, busPointers, buffer.getNumSamples());

    constexpr float decay = 0.90f;

    // Bus 0 is always "bus:Master" per BusRoutingTable::createDefault().
    if (! busViews.empty() && busViews[0].getNumChannels() > 0)
    {
        auto& masterBuf = busViews[0];
        const float blockPeakL = masterBuf.getMagnitude(0, 0, masterBuf.getNumSamples());
        const float blockPeakR = masterBuf.getNumChannels() > 1
            ? masterBuf.getMagnitude(1, 0, masterBuf.getNumSamples())
            : blockPeakL;

        masterPeakL.store(juce::jmax(blockPeakL, masterPeakL.load(std::memory_order_relaxed) * decay), std::memory_order_relaxed);
        masterPeakR.store(juce::jmax(blockPeakR, masterPeakR.load(std::memory_order_relaxed) * decay), std::memory_order_relaxed);
    }

    // Per-bus mono peak (max of that bus' own channels) for the Global
    // Mixer's per-channel strip meters.
    for (size_t i = 0; i < busViews.size() && i < busPeaks.size(); ++i)
    {
        auto& buf = busViews[i];
        if (buf.getNumChannels() <= 0)
            continue;

        float blockPeak = buf.getMagnitude(0, 0, buf.getNumSamples());
        if (buf.getNumChannels() > 1)
            blockPeak = juce::jmax(blockPeak, buf.getMagnitude(1, 0, buf.getNumSamples()));

        busPeaks[i].store(juce::jmax(blockPeak, busPeaks[i].load(std::memory_order_relaxed) * decay), std::memory_order_relaxed);
    }
}

void DrumSampleLoaderAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto kit = engine.getKit())
    {
        auto json = dsl::Preset::toJsonString(*kit, false);
        juce::MemoryOutputStream stream(destData, false);
        stream.writeText(json, false, false, "\n");
    }
}

void DrumSampleLoaderAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto jsonText = juce::String::createStringFromData(data, sizeInBytes);
    auto kit = std::make_shared<dsl::Kit>(dsl::Preset::fromJsonString(jsonText));
    dsl::DefaultKitFactory::ensureRequiredArticulations(*kit);

    // A host/standalone state contains the kit and its sample paths, but never
    // the audio buffers themselves. Hydrate those paths before publishing the
    // restored kit so the sample cells and the audible engine cannot get out
    // of sync after reopening a session.
    loadSamplesIntoKit(*kit, {}, false);
    engine.setKit(kit);
}

DrumSampleLoaderAudioProcessor::PresetLoadResult
DrumSampleLoaderAudioProcessor::loadSamplesIntoKit(dsl::Kit& kit,
                                                    const juce::File& presetDirectory,
                                                    bool storeResolvedAbsolutePaths)
{
    PresetLoadResult result;

    for (auto& inst : kit.instruments)
    {
        for (auto& articulation : inst.articulations)
        {
            for (auto& layer : articulation.velocityLayers)
            {
                for (auto& slot : layer.slots)
                {
                    for (auto& [channelId, sampleRef] : slot.channelSamples)
                    {
                        juce::ignoreUnused(channelId);
                        if (sampleRef.sourcePath.isEmpty())
                            continue;

                        const juce::File sampleFile = juce::File::isAbsolutePath(sampleRef.sourcePath)
                            ? juce::File(sampleRef.sourcePath)
                            : presetDirectory.getChildFile(sampleRef.sourcePath);

                        double sourceSampleRate = 44100.0;
                        if (auto loaded = samplePool.loadOrGet(sampleFile.getFullPathName(), sourceSampleRate))
                        {
                            sampleRef.buffer = loaded;
                            sampleRef.sourceSampleRate = sourceSampleRate;
                            if (storeResolvedAbsolutePaths)
                                sampleRef.sourcePath = sampleFile.getFullPathName();
                            ++result.loadedSamples;
                        }
                        else
                            result.missingSamplePaths.addIfNotAlreadyThere(sampleRef.sourcePath);
                    }
                }
            }
        }
    }

    return result;
}

DrumSampleLoaderAudioProcessor::PresetLoadResult
DrumSampleLoaderAudioProcessor::loadKitFromPresetFile(const juce::File& presetFile,
                                                       const juce::File& searchDirectory)
{
    PresetLoadResult result;
    auto kit = std::make_shared<dsl::Kit>(dsl::Preset::fromJsonString(presetFile.loadFileAsString()));
    auto baseDir = presetFile.getParentDirectory();
    dsl::DefaultKitFactory::ensureRequiredArticulations(*kit);

    if (searchDirectory.isDirectory())
    {
        auto relink = dsl::SamplePathRelinker::relinkMissingSamples(*kit, baseDir, searchDirectory);
        result.relinkedSamples = relink.relinked;
        result.ambiguousSamples = relink.ambiguous;
    }

    auto sampleLoadResult = loadSamplesIntoKit(*kit, baseDir, true);
    result.loadedSamples = sampleLoadResult.loadedSamples;
    result.missingSamplePaths = std::move(sampleLoadResult.missingSamplePaths);

    engine.setKit(kit);
    return result;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumSampleLoaderAudioProcessor();
}
