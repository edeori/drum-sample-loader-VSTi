#include "Engine/DrumEngine.h"
#include "Engine/DefaultKitFactory.h"
#include <juce_audio_basics/juce_audio_basics.h>

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

class DrumEngineTests : public juce::UnitTest
{
public:
    DrumEngineTests() : juce::UnitTest("DrumEngine", "Engine") {}

    void runTest() override
    {
        beginTest("A channel routed to a bus the host doesn't expose falls back to Master");
        {
            auto kit = std::make_shared<Kit>(DefaultKitFactory::createDefaultKit());
            auto* kick = kit->findInstrumentById("kick");
            expect(kick != nullptr);
            auto* main = kick->primaryArticulation();
            expect(main != nullptr);

            RoundRobinSlot slot;
            slot.channelSamples["In"].buffer = makeConstantBuffer(1, 200, 1.0f);
            slot.channelSamples["In"].sourceSampleRate = 44100.0;
            for (auto& layer : main->velocityLayers)
                layer.slots.push_back(slot);

            const int kickInBusIndex = kit->busRouting.indexOfBus(kick->getChannelRouting("In"));
            const int masterBusIndex = kit->busRouting.indexOfBus("bus:Master");
            expect(kickInBusIndex >= 0 && kickInBusIndex != masterBusIndex,
                   "test setup assumption: Kick's In channel is routed to a dedicated, non-Master bus by default");

            DrumEngine engine;
            engine.prepare(44100.0, 512);
            engine.setKit(kit);

            // Simulate a plain 2-channel Standalone output device: only
            // Master actually has real channels this session, everything
            // else is disabled by the host the way an unavailable bus is.
            const int numBuses = (int) kit->busRouting.buses.size();
            std::vector<juce::AudioBuffer<float>> busStorage((size_t) numBuses);
            std::vector<juce::AudioBuffer<float>*> busBuffers;
            for (int i = 0; i < numBuses; ++i)
            {
                busStorage[(size_t) i].setSize(i == masterBusIndex ? 2 : 0, 64);
                busStorage[(size_t) i].clear();
                busBuffers.push_back(&busStorage[(size_t) i]);
            }

            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, main->midiNote, (juce::uint8) 100), 0);
            engine.processBlock(midi, busBuffers, 64);

            expectGreaterThan(busStorage[(size_t) masterBusIndex].getMagnitude(0, 0, 64), 0.0f,
                              "Kick's hit should have fallen back onto the Master bus");
            expectEquals(busStorage[(size_t) kickInBusIndex].getNumSamples(), 64);
            expectEquals(busStorage[(size_t) kickInBusIndex].getNumChannels(), 0);
        }

        beginTest("Master fader trims a bus other than bus:Master");
        {
            auto kit = std::make_shared<Kit>(DefaultKitFactory::createDefaultKit());
            kit->masterDb = -100.0f; // effectively silent
            auto* kick = kit->findInstrumentById("kick");
            auto* main = kick->primaryArticulation();

            RoundRobinSlot slot;
            slot.channelSamples["In"].buffer = makeConstantBuffer(1, 200, 1.0f);
            slot.channelSamples["In"].sourceSampleRate = 44100.0;
            for (auto& layer : main->velocityLayers)
                layer.slots.push_back(slot);

            const int kickInBusIndex = kit->busRouting.indexOfBus(kick->getChannelRouting("In"));

            DrumEngine engine;
            engine.prepare(44100.0, 512);
            engine.setKit(kit);

            const int numBuses = (int) kit->busRouting.buses.size();
            std::vector<juce::AudioBuffer<float>> busStorage((size_t) numBuses);
            std::vector<juce::AudioBuffer<float>*> busBuffers;
            for (int i = 0; i < numBuses; ++i)
            {
                busStorage[(size_t) i].setSize(2, 64);
                busStorage[(size_t) i].clear();
                busBuffers.push_back(&busStorage[(size_t) i]);
            }

            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, main->midiNote, (juce::uint8) 100), 0);
            engine.processBlock(midi, busBuffers, 64);

            expectWithinAbsoluteError(busStorage[(size_t) kickInBusIndex].getMagnitude(0, 0, 64), 0.0f, 0.0001f,
                                      "Master fader at -100 dB should silence Kick even though it's routed to its own bus");
        }

        beginTest("A Spot channel's swap toggle swaps a genuinely stereo sample's channels outright");
        {
            auto kit = std::make_shared<Kit>(DefaultKitFactory::createDefaultKit());
            auto* tom1 = kit->findInstrumentById("tom1");
            expect(tom1 != nullptr);
            tom1->channelSwapLR["Spot"] = true;

            auto stereoSample = std::make_shared<juce::AudioBuffer<float>>(2, 200);
            for (int i = 0; i < stereoSample->getNumSamples(); ++i)
            {
                stereoSample->setSample(0, i, 0.3f);
                stereoSample->setSample(1, i, 0.7f);
            }

            auto* main = tom1->primaryArticulation();
            RoundRobinSlot slot;
            slot.channelSamples["Spot"].buffer = stereoSample;
            slot.channelSamples["Spot"].sourceSampleRate = 44100.0;
            for (auto& layer : main->velocityLayers)
                layer.slots.push_back(slot);

            DrumEngine engine;
            engine.prepare(44100.0, 512);
            engine.setKit(kit);

            const int numBuses = (int) kit->busRouting.buses.size();
            std::vector<juce::AudioBuffer<float>> busStorage((size_t) numBuses);
            std::vector<juce::AudioBuffer<float>*> busBuffers;
            for (int i = 0; i < numBuses; ++i)
            {
                busStorage[(size_t) i].setSize(2, 64);
                busStorage[(size_t) i].clear();
                busBuffers.push_back(&busStorage[(size_t) i]);
            }

            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, main->midiNote, (juce::uint8) 100), 0);
            engine.processBlock(midi, busBuffers, 64);

            const int tomBusIndex = kit->busRouting.indexOfBus(tom1->getChannelRouting("Spot"));
            expectWithinAbsoluteError(busStorage[(size_t) tomBusIndex].getSample(0, 0), 0.7f, 0.0001f,
                                      "channel 0 should carry the source's right channel once swapped");
            expectWithinAbsoluteError(busStorage[(size_t) tomBusIndex].getSample(1, 0), 0.3f, 0.0001f,
                                      "channel 1 should carry the source's left channel once swapped");
        }
    }
};

static DrumEngineTests drumEngineTestsInstance;
