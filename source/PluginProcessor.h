#pragma once

#include "Engine/BusRoutingTable.h"
#include "Engine/DrumEngine.h"
#include "Engine/Kit.h"
#include "Engine/SamplePool.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

class DrumSampleLoaderAudioProcessor : public juce::AudioProcessor
{
public:
    DrumSampleLoaderAudioProcessor();
    ~DrumSampleLoaderAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; } // "let it ring" — long natural decays

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    struct PresetLoadResult
    {
        int loadedSamples = 0;
        int relinkedSamples = 0;
        int ambiguousSamples = 0;
        juce::StringArray missingSamplePaths;
    };

    /** Message-thread only. When searchDirectory is supplied, missing sample
        paths are repaired by recursively finding exact filenames below it. */
    PresetLoadResult loadKitFromPresetFile(const juce::File& presetFile,
                                           const juce::File& searchDirectory = {});

    dsl::DrumEngine& getEngine() { return engine; }
    dsl::SamplePool& getSamplePool() { return samplePool; }

    /** Message-thread only: injects a synthetic note-on for editor "preview /
        audition" buttons, via the same MidiMessageCollector mechanism JUCE's
        own on-screen MIDI keyboard uses — thread-safe, no audio-thread locks.
    */
    void triggerPreviewNote(int midiNote, int velocity);

    // Post-fader Master bus peak level, updated every block; UI polls these
    // at its own refresh rate to animate the master meter.
    float getMasterPeakL() const { return masterPeakL.load(std::memory_order_relaxed); }
    float getMasterPeakR() const { return masterPeakR.load(std::memory_order_relaxed); }

    // Post-fader peak level per output bus (mono — max of that bus' channels),
    // updated every block; the Global Mixer polls this per channel strip so
    // each fader can show what's actually coming out of its assigned bus.
    float getBusPeak(int busIndex) const
    {
        return busIndex >= 0 && busIndex < (int) busPeaks.size()
            ? busPeaks[(size_t) busIndex].load(std::memory_order_relaxed)
            : 0.0f;
    }

    // Post-fader, pre-bus-mix peak for one originating mic channel — see
    // DrumEngine::getChannelPeak/meterKeyFor. This is what the Global Mixer's
    // per-strip meters poll, so a channel's meter reflects only its own hits
    // even when several channels share the same output bus.
    float getChannelPeak(uint64_t meterKey) const { return engine.getChannelPeak(meterKey); }

private:
    static BusesProperties buildBusesProperties();

    PresetLoadResult loadSamplesIntoKit(dsl::Kit& kit,
                                        const juce::File& presetDirectory,
                                        bool storeResolvedAbsolutePaths);

    dsl::DrumEngine engine;
    juce::AudioFormatManager formatManager;
    dsl::SamplePool samplePool { formatManager };
    juce::MidiMessageCollector previewMidiCollector;

    std::atomic<float> masterPeakL { 0.0f };
    std::atomic<float> masterPeakR { 0.0f };
    std::vector<std::atomic<float>> busPeaks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumSampleLoaderAudioProcessor)
};
