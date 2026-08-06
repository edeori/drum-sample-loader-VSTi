#include "Engine/Kit.h"
#include "Engine/DefaultKitFactory.h"
#include <juce_core/juce_core.h>
#include <set>

using namespace dsl;

class KitMidiMapTests : public juce::UnitTest
{
public:
    KitMidiMapTests() : juce::UnitTest("Kit MIDI map", "Engine") {}

    void runTest() override
    {
        beginTest("Default kit has a note for every articulation, with no duplicates");
        {
            auto kit = DefaultKitFactory::createDefaultKit();
            std::set<int> seen;
            for (auto& inst : kit.instruments)
            {
                expect(! inst.articulations.empty(), "instrument " + inst.id + " has no articulations");
                for (auto& articulation : inst.articulations)
                {
                    expect(articulation.midiNote >= 0, inst.id + "/" + articulation.id + " has no MIDI note assigned");
                    expect(seen.find(articulation.midiNote) == seen.end(),
                           "duplicate MIDI note in default kit: " + juce::String(articulation.midiNote));
                    seen.insert(articulation.midiNote);
                }
            }
        }

        beginTest("Default cymbals expose the required articulations");
        {
            auto kit = DefaultKitFactory::createDefaultKit();
            auto* hihat = kit.findInstrumentById("hihat");
            auto* crash1 = kit.findInstrumentById("crash1");
            auto* ride = kit.findInstrumentById("ride");
            auto* china = kit.findInstrumentById("china");
            expect(hihat != nullptr && hihat->findArticulation("closed") != nullptr);
            expect(hihat != nullptr && hihat->findArticulation("open1") != nullptr);
            expect(hihat != nullptr && hihat->findArticulation("tip_open") != nullptr);
            expect(crash1 != nullptr && crash1->findArticulation("left_choke") != nullptr);
            expect(crash1 != nullptr && crash1->findArticulation("left_choke")->action == ArticulationAction::Choke);
            expect(ride != nullptr && ride->findArticulation("main") != nullptr);
            expect(ride != nullptr && ride->findArticulation("bell") != nullptr);
            expect(ride != nullptr && ride->findArticulation("bell")->action == ArticulationAction::Trigger);
            expectEquals(ride->findArticulation("bell")->midiNote, 56);
            expect(china != nullptr && china->findArticulation("left_hit") != nullptr);
            expect(china != nullptr && china->findArticulation("left_choke") != nullptr);
            expect(china != nullptr && china->findArticulation("left_choke")->action == ArticulationAction::Choke);
            expectEquals(china->findArticulation("left_choke")->midiNote, 60);
        }

        beginTest("Every default instrument exposes a spot channel");
        {
            auto kit = DefaultKitFactory::createDefaultKit();
            for (auto& instrument : kit.instruments)
            {
                bool hasSpot = false;
                for (auto& channel : instrument.micChannels)
                    hasSpot = hasSpot || channel.role == MicRole::Spot;
                expect(hasSpot, instrument.name + " has no spot channel");
            }
        }

        beginTest("Overhead and room formats can be selected independently");
        {
            auto instrument = DefaultKitFactory::createBlankInstrument(
                "hybrid", "Hybrid", InstrumentCategory::Cymbal, 70,
                AmbienceFormat::Stereo, AmbienceFormat::MonoPair);

            const auto* overhead = instrument.findChannel("OH");
            expect(overhead != nullptr);
            expect(overhead != nullptr && overhead->numChannels == 2);
            expect(instrument.findChannel("OH_L") == nullptr);
            expectWithinAbsoluteError(instrument.findChannel("Room_L")->pan, -1.0f, 0.0001f);
            expectWithinAbsoluteError(instrument.findChannel("Room_R")->pan, 1.0f, 0.0001f);
        }

        beginTest("Existing ambience channels can be reconfigured before samples are assigned");
        {
            auto instrument = DefaultKitFactory::createBlankInstrument(
                "convert", "Convert", InstrumentCategory::Cymbal, 70);

            expect(DefaultKitFactory::reconfigureAmbienceChannels(
                instrument, MicRole::Overhead, AmbienceFormat::Stereo));
            expect(instrument.findChannel("OH") != nullptr && instrument.findChannel("OH")->numChannels == 2);
            expect(instrument.findChannel("OH_L") == nullptr);
            expect(instrument.findChannel("Room_L") != nullptr);

            RoundRobinSlot slot;
            SampleRef sample;
            sample.sourcePath = "assigned.wav";
            slot.channelSamples["OH"] = sample;
            instrument.primaryArticulation()->velocityLayers.resize(1);
            instrument.primaryArticulation()->velocityLayers.front().slots.push_back(std::move(slot));
            expect(DefaultKitFactory::reconfigureAmbienceChannels(
                instrument, MicRole::Overhead, AmbienceFormat::MonoPair));
            expect(instrument.findChannel("OH") == nullptr);
            expect(instrument.findChannel("OH_L") != nullptr);
            // The inactive stereo mapping is retained and becomes active
            // again if the user switches the channel topology back.
            expect(instrument.primaryArticulation()->velocityLayers.front().slots.front()
                       .channelSamples.find("OH")
                   != instrument.primaryArticulation()->velocityLayers.front().slots.front().channelSamples.end());
        }

        beginTest("Supplementary Room M survives L/R and stereo format changes");
        {
            auto instrument = DefaultKitFactory::createBlankInstrument(
                "china", "China", InstrumentCategory::Cymbal, 52);
            instrument.micChannels.push_back({ "Room_M", "Room M", MicRole::Room, 1, 0.0f });
            instrument.channelFaderDb["Room_M"] = -3.0f;

            expect(DefaultKitFactory::reconfigureAmbienceChannels(
                instrument, MicRole::Room, AmbienceFormat::MonoPair));
            expect(instrument.findChannel("Room_M") != nullptr);
            expect(DefaultKitFactory::reconfigureAmbienceChannels(
                instrument, MicRole::Room, AmbienceFormat::Stereo));
            expect(instrument.findChannel("Room") != nullptr);
            expect(instrument.findChannel("Room_M") != nullptr);
            expectWithinAbsoluteError(instrument.getChannelFaderDb("Room_M"), -3.0f, 0.0001f);
        }

        beginTest("instrumentForMidiNote resolves the correct instrument");
        {
            auto kit = DefaultKitFactory::createDefaultKit();
            auto* snare = kit.instrumentForMidiNote(27);
            expect(snare != nullptr && snare->id == "snare");
        }

        beginTest("Assigning an already-taken note without force reports Conflict and changes nothing");
        {
            auto kit = DefaultKitFactory::createDefaultKit();
            const int kicksNote = kit.findInstrumentById("kick")->primaryArticulation()->midiNote;

            auto result = kit.setInstrumentMidiNote("snare", kicksNote, false);
            expect(result == Kit::SetNoteResult::Conflict);
            expectEquals(kit.findInstrumentById("snare")->primaryArticulation()->midiNote, 27);
            expectEquals(kit.findInstrumentById("kick")->primaryArticulation()->midiNote, kicksNote);
        }

        beginTest("Assigning an already-taken note with force reassigns and clears the previous owner");
        {
            auto kit = DefaultKitFactory::createDefaultKit();
            const int kicksNote = kit.findInstrumentById("kick")->primaryArticulation()->midiNote;

            auto result = kit.setInstrumentMidiNote("snare", kicksNote, true);
            expect(result == Kit::SetNoteResult::Ok);
            expectEquals(kit.findInstrumentById("snare")->primaryArticulation()->midiNote, kicksNote);
            expectEquals(kit.findInstrumentById("kick")->primaryArticulation()->midiNote, -1);
        }

        beginTest("Articulation MIDI reassignment is conflict checked across the whole kit");
        {
            auto kit = DefaultKitFactory::createDefaultKit();
            const int chinaLeftHitNote = kit.findInstrumentById("china")->findArticulation("left_hit")->midiNote;
            auto result = kit.setArticulationMidiNote("hihat", "open1", chinaLeftHitNote, false);
            expect(result == Kit::SetNoteResult::Conflict);
            expectEquals(kit.findInstrumentById("hihat")->findArticulation("open1")->midiNote, 39);
        }

        beginTest("Multiple articulations may be unassigned without conflicting");
        {
            auto kit = DefaultKitFactory::createDefaultKit();
            expect(kit.setArticulationMidiNote("hihat", "open1", -1, false) == Kit::SetNoteResult::Ok);
            expect(kit.setArticulationMidiNote("hihat", "open2", -1, false) == Kit::SetNoteResult::Ok);
            expectEquals(kit.findInstrumentById("hihat")->findArticulation("open1")->midiNote, -1);
            expectEquals(kit.findInstrumentById("hihat")->findArticulation("open2")->midiNote, -1);
        }

        beginTest("Legacy Hi-hat and Crash Main grids gain the required articulations");
        {
            Kit kit;
            auto hihat = DefaultKitFactory::createBlankInstrument(
                "hihat", "Hi-hat", InstrumentCategory::Cymbal, 42);
            auto crash = DefaultKitFactory::createBlankInstrument(
                "crash1", "Crash 1", InstrumentCategory::Cymbal, 49);
            hihat.articulations.front().velocityLayers.resize((size_t) kit.velocityConfig.getLayerCount());
            crash.articulations.front().velocityLayers.resize((size_t) kit.velocityConfig.getLayerCount());

            RoundRobinSlot chokeSlot;
            SampleRef chokeSample;
            chokeSample.sourcePath = "Crash 1 2_1 choke - Overheads ST.wav";
            chokeSlot.channelSamples["OH"] = chokeSample;
            crash.articulations.front().velocityLayers.front().slots.push_back(std::move(chokeSlot));

            kit.instruments.push_back(std::move(hihat));
            kit.instruments.push_back(std::move(crash));
            DefaultKitFactory::ensureRequiredArticulations(kit);

            auto* upgradedHihat = kit.findInstrumentById("hihat");
            auto* upgradedCrash = kit.findInstrumentById("crash1");
            expect(upgradedHihat->findArticulation("closed") != nullptr);
            expect(upgradedHihat->findArticulation("half_open") != nullptr);
            expect(upgradedHihat->findArticulation("open") != nullptr);
            expect(upgradedCrash->findArticulation("hit") != nullptr);
            expect(upgradedCrash->findArticulation("choke") != nullptr);
            expect(upgradedCrash->findArticulation("choke")->action == ArticulationAction::Choke);
            expectEquals((int) upgradedCrash->findArticulation("hit")->velocityLayers.front().slots.size(), 0);
            expectEquals((int) upgradedCrash->findArticulation("choke")->velocityLayers.front().slots.size(), 1);
        }

        beginTest("Legacy Hi-hat Open samples migrate to Open rather than Closed");
        {
            Kit kit;
            auto hihat = DefaultKitFactory::createBlankInstrument(
                "hihat", "Hi-hat", InstrumentCategory::Cymbal, 42);
            hihat.articulations.front().velocityLayers.resize((size_t) kit.velocityConfig.getLayerCount());
            RoundRobinSlot slot;
            SampleRef sample;
            sample.sourcePath = "Hi-hat 4_1 (open)-OH L.wav";
            slot.channelSamples["OH_L"] = sample;
            hihat.articulations.front().velocityLayers[1].slots.push_back(std::move(slot));
            kit.instruments.push_back(std::move(hihat));

            DefaultKitFactory::ensureRequiredArticulations(kit);
            auto* upgraded = kit.findInstrumentById("hihat");
            expectEquals(upgraded->articulations.front().id, juce::String("closed"));
            expectEquals(upgraded->findArticulation("open")->midiNote, 46);
            expectEquals((int) upgraded->findArticulation("closed")->velocityLayers[1].slots.size(), 0);
            expectEquals((int) upgraded->findArticulation("open")->velocityLayers[1].slots.size(), 1);
        }

        beginTest("Legacy China Choke and Ride Bell samples migrate to their articulations");
        {
            Kit kit;
            auto china = DefaultKitFactory::createBlankInstrument(
                "china", "China", InstrumentCategory::Cymbal, 52);
            auto ride = DefaultKitFactory::createBlankInstrument(
                "ride", "Ride", InstrumentCategory::Cymbal, 51);
            china.articulations.front().velocityLayers.resize((size_t) kit.velocityConfig.getLayerCount());
            ride.articulations.front().velocityLayers.resize((size_t) kit.velocityConfig.getLayerCount());

            RoundRobinSlot chinaChokeSlot;
            chinaChokeSlot.channelSamples["OH"].sourcePath = "China 1 (chk) - OH ST.wav";
            china.articulations.front().velocityLayers.front().slots.push_back(std::move(chinaChokeSlot));

            RoundRobinSlot rideBellSlot;
            rideBellSlot.channelSamples["Spot"].sourcePath = "Ride bell 3 - Spot.wav";
            ride.articulations.front().velocityLayers.front().slots.push_back(std::move(rideBellSlot));

            kit.instruments.push_back(std::move(china));
            kit.instruments.push_back(std::move(ride));
            DefaultKitFactory::ensureRequiredArticulations(kit);

            auto* upgradedChina = kit.findInstrumentById("china");
            auto* upgradedRide = kit.findInstrumentById("ride");
            expect(upgradedChina->findArticulation("hit") != nullptr);
            expect(upgradedChina->findArticulation("choke") != nullptr);
            expect(upgradedChina->findArticulation("choke")->action == ArticulationAction::Choke);
            expectEquals((int) upgradedChina->findArticulation("hit")->velocityLayers.front().slots.size(), 0);
            expectEquals((int) upgradedChina->findArticulation("choke")->velocityLayers.front().slots.size(), 1);
            expect(upgradedRide->findArticulation("hit") != nullptr);
            expect(upgradedRide->findArticulation("bell") != nullptr);
            expect(upgradedRide->findArticulation("bell")->action == ArticulationAction::Trigger);
            expectEquals((int) upgradedRide->findArticulation("hit")->velocityLayers.front().slots.size(), 0);
            expectEquals((int) upgradedRide->findArticulation("bell")->velocityLayers.front().slots.size(), 1);
        }
    }
};

static KitMidiMapTests kitMidiMapTestsInstance;
