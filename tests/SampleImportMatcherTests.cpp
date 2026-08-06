#include "Engine/DefaultKitFactory.h"
#include "Engine/SampleImportMatcher.h"
#include <juce_core/juce_core.h>

using namespace dsl;

class SampleImportMatcherTests : public juce::UnitTest
{
public:
    SampleImportMatcherTests() : juce::UnitTest("Sample import matching", "Engine") {}

    void runTest() override
    {
        auto instrument = DefaultKitFactory::createBlankInstrument(
            "snare", "Snare", InstrumentCategory::Body, 38,
            AmbienceFormat::Stereo, AmbienceFormat::Stereo);

        beginTest("Standalone stereo ambience filenames are recognized");
        {
            auto oh = SampleImportMatcher::match("OH", instrument);
            auto overhead = SampleImportMatcher::match("Overhead", instrument);
            auto overheads = SampleImportMatcher::match("Overheads", instrument);
            auto room = SampleImportMatcher::match("Room", instrument);
            expect(oh.has_value() && oh->channelId == "OH");
            expect(overhead.has_value() && overhead->channelId == "OH");
            expect(overheads.has_value() && overheads->channelId == "OH");
            expect(room.has_value() && room->channelId == "Room");
            expect(oh.has_value() && oh->variationKey.isEmpty());
            expect(room.has_value() && room->variationKey.isEmpty());
        }

        beginTest("Underscore names group stereo OH and Room into the same take");
        {
            auto oh = SampleImportMatcher::match("Snare_OH_01", instrument);
            auto room = SampleImportMatcher::match("Snare_Room_01", instrument);
            expect(oh.has_value() && oh->channelId == "OH");
            expect(room.has_value() && room->channelId == "Room");
            expect(oh.has_value() && room.has_value() && oh->variationKey == room->variationKey);
        }

        beginTest("Explicit Stereo labels and the original dash format work");
        {
            auto oh = SampleImportMatcher::match("Snare 02-Overhead Stereo", instrument);
            auto room = SampleImportMatcher::match("Snare 02-Room Stereo", instrument);
            auto spot = SampleImportMatcher::match("Snare 02-Snare Spot", instrument);
            expect(oh.has_value() && oh->channelId == "OH");
            expect(room.has_value() && room->channelId == "Room");
            expect(spot.has_value() && spot->channelId == "Spot");
            expect(oh.has_value() && room.has_value() && oh->variationKey == room->variationKey);
            expect(oh.has_value() && spot.has_value() && oh->variationKey == spot->variationKey);
        }

        beginTest("ST is accepted as an explicit stereo marker");
        {
            auto oh = SampleImportMatcher::match("Snare_03_Overheads_ST", instrument);
            auto room = SampleImportMatcher::match("Snare_03_Room_ST", instrument);
            expect(oh.has_value() && oh->channelId == "OH");
            expect(room.has_value() && room->channelId == "Room");
            expect(SampleImportMatcher::hasStereoMarker("Snare_03_Overheads_ST"));
            expect(SampleImportMatcher::hasStereoMarker("take-Room-Stereo"));
            expect(! SampleImportMatcher::hasStereoMarker("Ghost note"));
        }

        beginTest("Dryblood Crash folder naming groups Overheads ST and Room ST");
        {
            auto oh = SampleImportMatcher::match("Crash 1 1_1 - Overheads ST", instrument);
            auto room = SampleImportMatcher::match("Crash 1 1_1 - Room ST", instrument);
            auto choke = SampleImportMatcher::match("Crash 1 2_1 choke - Overheads ST", instrument);
            expect(oh.has_value() && oh->channelId == "OH");
            expect(room.has_value() && room->channelId == "Room");
            expect(choke.has_value() && choke->channelId == "OH");
            expect(oh.has_value() && room.has_value() && oh->variationKey == room->variationKey);
            expect(choke.has_value() && choke->variationKey.contains("CHOKE"));
        }

        beginTest("Dryblood chk shorthand targets Choke and preserves explicit selection");
        {
            // A literal Hit/Choke pair (rather than the default kit's china,
            // which now has positional Left/Right Hit/Choke zones per
            // docs/05-midi-mapping.md) — this test is about
            // targetArticulationId()'s generic choke-marker redirect, not
            // about china's specific default shape.
            auto china = DefaultKitFactory::createBlankInstrument(
                "china", "China", InstrumentCategory::Cymbal, 52);
            china.articulations.front().id = "hit";
            china.articulations.front().name = "Hit";
            InstrumentArticulation choke;
            choke.id = "choke";
            choke.name = "Choke";
            choke.action = ArticulationAction::Choke;
            china.articulations.push_back(choke);

            const auto chkName = juce::String("China 4_2 (chk)-OH R");
            china.micChannels.push_back({ "Room_M", "Room M", MicRole::Room, 1, 0.0f });
            const auto oh = SampleImportMatcher::match(chkName, china);
            const auto room = SampleImportMatcher::match("China 4_2 (chk)-Room R", china);
            const auto roomMid = SampleImportMatcher::match("China 4_2 (chk)-Room M", china);
            const auto spot = SampleImportMatcher::match("China 4_2 (chk)-Spot", china);
            expect(oh.has_value() && oh->channelId == "OH_R");
            expect(room.has_value() && room->channelId == "Room_R");
            expect(roomMid.has_value() && roomMid->channelId == "Room_M");
            expect(spot.has_value() && spot->channelId == "Spot");
            expect(oh.has_value() && room.has_value() && oh->variationKey == room->variationKey);
            expect(oh.has_value() && roomMid.has_value() && oh->variationKey == roomMid->variationKey);
            expect(oh.has_value() && spot.has_value() && oh->variationKey == spot->variationKey);
            expect(SampleImportMatcher::hasChokeMarker(chkName));
            expect(SampleImportMatcher::hasRoomMidMarker("China 4_2 (chk)-Room M"));
            expectEquals(SampleImportMatcher::targetArticulationId(chkName, china, "hit"),
                         juce::String("choke"));
            expectEquals(SampleImportMatcher::targetArticulationId(
                             "China 4_2-OH R", china, "choke"),
                         juce::String("choke"));
            expectEquals(SampleImportMatcher::targetArticulationId(
                             "China 4_2-OH R", china, "hit"),
                         juce::String("hit"));
        }

        beginTest("Unrelated files are not imported silently");
        {
            expect(! SampleImportMatcher::match("Readme", instrument).has_value());
        }

        beginTest("A split L/R layout requires explicit left or right filenames");
        {
            auto split = DefaultKitFactory::createBlankInstrument(
                "snare", "Snare", InstrumentCategory::Body, 38,
                AmbienceFormat::MonoPair, AmbienceFormat::MonoPair);
            auto left = SampleImportMatcher::match("Snare_01_OH_L", split);
            auto right = SampleImportMatcher::match("Snare_01_OH_R", split);
            expect(left.has_value() && left->channelId == "OH_L");
            expect(right.has_value() && right->channelId == "OH_R");
            expect(! SampleImportMatcher::match("Snare_01_OH", split).has_value());
        }
    }
};

static SampleImportMatcherTests sampleImportMatcherTestsInstance;
