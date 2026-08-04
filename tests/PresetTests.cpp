#include "Engine/Preset.h"
#include "Engine/DefaultKitFactory.h"
#include <juce_core/juce_core.h>

using namespace dsl;

class PresetTests : public juce::UnitTest
{
public:
    PresetTests() : juce::UnitTest("Preset serialization", "Engine") {}

    void runTest() override
    {
        beginTest("Round-trips a populated kit through the docs/06 JSON schema");
        {
            auto kit = DefaultKitFactory::createDefaultKit();
            kit.kitName = "Round Trip Kit";
            kit.masterDb = -1.5f;
            kit.masterOverheadDb = -3.0f;
            kit.masterRoomDb = -6.0f;

            auto* snare = kit.findInstrumentById("snare");
            expect(snare != nullptr);
            snare->channelFaderDb["Top"] = -2.0f;
            snare->findChannel("Top")->pan = 0.35f;

            RoundRobinSlot slot;
            SampleRef ref;
            ref.sourcePath = "samples/snare/heavy_hit1_top.wav";
            slot.channelSamples["Top"] = ref;
            snare->primaryArticulation()->velocityLayers.back().slots.push_back(slot);

            const auto json = Preset::toJsonString(kit);
            auto loaded = Preset::fromJsonString(json);

            expectEquals(loaded.kitName, kit.kitName);
            expectWithinAbsoluteError(loaded.masterDb, kit.masterDb, 0.0001f);
            expectWithinAbsoluteError(loaded.masterOverheadDb, kit.masterOverheadDb, 0.0001f);
            expectWithinAbsoluteError(loaded.masterRoomDb, kit.masterRoomDb, 0.0001f);
            expectEquals((int) loaded.instruments.size(), (int) kit.instruments.size());
            expectEquals((int) loaded.velocityConfig.thresholds.size(), (int) kit.velocityConfig.thresholds.size());

            auto* loadedSnare = loaded.findInstrumentById("snare");
            expect(loadedSnare != nullptr);
            expectWithinAbsoluteError(loadedSnare->getChannelFaderDb("Top"), -2.0f, 0.0001f);
            expectWithinAbsoluteError(loadedSnare->findChannel("Top")->pan, 0.35f, 0.0001f);
            expectEquals((int) loadedSnare->primaryArticulation()->velocityLayers.back().slots.size(), 1);
            expectEquals(loadedSnare->primaryArticulation()->velocityLayers.back().slots[0].channelSamples.at("Top").sourcePath,
                         juce::String("samples/snare/heavy_hit1_top.wav"));

            auto* loadedHihat = loaded.findInstrumentById("hihat");
            expect(loadedHihat != nullptr);
            expectEquals((int) loadedHihat->articulations.size(), 3);
            expectEquals(loadedHihat->findArticulation("half_open")->midiNote, 44);
            expectWithinAbsoluteError(loadedHihat->findChannel("OH_L")->pan, -1.0f, 0.0001f);
            expectWithinAbsoluteError(loadedHihat->findChannel("OH_R")->pan, 1.0f, 0.0001f);
        }


        beginTest("Version 1 presets migrate their trigger, spot and hard pans");
        {
            const juce::String legacyJson = R"json({
                "presetVersion": 1,
                "kitName": "Legacy",
                "globalVelocityLayers": {
                    "layerNames": ["Soft"],
                    "thresholds": []
                },
                "instruments": [{
                    "id": "legacy_cymbal",
                    "name": "Legacy Cymbal",
                    "category": "Cymbal",
                    "midiNote": 60,
                    "velocityLayers": [{
                        "layerIndex": 0,
                        "roundRobinSlots": []
                    }],
                    "micChannels": [
                        { "id": "OH_L", "displayName": "Overhead L", "role": "Overhead", "numChannels": 1 },
                        { "id": "OH_R", "displayName": "Overhead R", "role": "Overhead", "numChannels": 1 },
                        { "id": "Room_L", "displayName": "Room L", "role": "Room", "numChannels": 1 },
                        { "id": "Room_R", "displayName": "Room R", "role": "Room", "numChannels": 1 }
                    ]
                }]
            })json";

            auto loaded = Preset::fromJsonString(legacyJson);
            auto* instrument = loaded.findInstrumentById("legacy_cymbal");
            expect(instrument != nullptr);
            expect(instrument != nullptr && instrument->primaryArticulation()->midiNote == 60);
            expect(instrument != nullptr && instrument->findChannel("Spot") != nullptr);
            expectWithinAbsoluteError(instrument->findChannel("OH_L")->pan, -1.0f, 0.0001f);
            expectWithinAbsoluteError(instrument->findChannel("OH_R")->pan, 1.0f, 0.0001f);
            expectWithinAbsoluteError(instrument->findChannel("Room_L")->pan, -1.0f, 0.0001f);
            expectWithinAbsoluteError(instrument->findChannel("Room_R")->pan, 1.0f, 0.0001f);
        }
    }
};

static PresetTests presetTestsInstance;
