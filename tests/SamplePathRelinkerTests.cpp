#include "Engine/DefaultKitFactory.h"
#include "Engine/SamplePathRelinker.h"
#include <juce_core/juce_core.h>

using namespace dsl;

namespace
{

SampleRef& addMissingSample(Kit& kit, const juce::String& path)
{
    auto* snare = kit.findInstrumentById("snare");
    auto* articulation = snare->primaryArticulation();
    RoundRobinSlot slot;
    slot.channelSamples["Top"].sourcePath = path;
    articulation->velocityLayers.front().slots.push_back(slot);
    return articulation->velocityLayers.front().slots.back().channelSamples["Top"];
}

} // namespace

class SamplePathRelinkerTests : public juce::UnitTest
{
public:
    SamplePathRelinkerTests() : juce::UnitTest("Sample path relinking", "Engine") {}

    void runTest() override
    {
        const auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("swarm-relink-tests", {}, false);
        expect(tempRoot.createDirectory());

        beginTest("Recursively finds a missing sample by exact filename");
        {
            auto kit = DefaultKitFactory::createDefaultKit();
            auto& sample = addMissingSample(kit, "/Old Volume/Drums/Snare/snare-hit.wav");
            const auto replacement = tempRoot.getChildFile("Moved/Deep/snare-hit.wav");
            expect(replacement.getParentDirectory().createDirectory());
            expect(replacement.replaceWithText("test"));

            const auto result = SamplePathRelinker::relinkMissingSamples(
                kit, tempRoot.getChildFile("Presets"), tempRoot);
            expectEquals(result.relinked, 1);
            expectEquals(result.ambiguous, 0);
            expectEquals(sample.sourcePath, replacement.getFullPathName());
        }

        beginTest("Uses the old parent hierarchy to disambiguate duplicate filenames");
        {
            auto kit = DefaultKitFactory::createDefaultKit();
            auto& sample = addMissingSample(kit, "/Old Volume/Pack/Crash 1/shared.wav");
            const auto correct = tempRoot.getChildFile("Library/Pack/Crash 1/shared.wav");
            const auto other = tempRoot.getChildFile("Library/Pack/Crash 2/shared.wav");
            expect(correct.getParentDirectory().createDirectory());
            expect(other.getParentDirectory().createDirectory());
            expect(correct.replaceWithText("correct"));
            expect(other.replaceWithText("other"));

            const auto result = SamplePathRelinker::relinkMissingSamples(
                kit, tempRoot.getChildFile("Presets"), tempRoot);
            expectEquals(result.relinked, 1);
            expectEquals(sample.sourcePath, correct.getFullPathName());
        }

        beginTest("Leaves equally plausible duplicate filenames unresolved");
        {
            auto kit = DefaultKitFactory::createDefaultKit();
            const juce::String original = "/Old Volume/Unknown/ambiguous.wav";
            auto& sample = addMissingSample(kit, original);
            const auto first = tempRoot.getChildFile("One/ambiguous.wav");
            const auto second = tempRoot.getChildFile("Two/ambiguous.wav");
            expect(first.getParentDirectory().createDirectory());
            expect(second.getParentDirectory().createDirectory());
            expect(first.replaceWithText("one"));
            expect(second.replaceWithText("two"));

            const auto result = SamplePathRelinker::relinkMissingSamples(
                kit, tempRoot.getChildFile("Presets"), tempRoot);
            expectEquals(result.relinked, 0);
            expectEquals(result.ambiguous, 1);
            expectEquals(sample.sourcePath, original);
            expectEquals(result.unresolvedPaths.size(), 1);
        }

        expect(tempRoot.deleteRecursively());
    }
};

static SamplePathRelinkerTests samplePathRelinkerTestsInstance;
