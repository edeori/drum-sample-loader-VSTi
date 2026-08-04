#include "Engine/DefaultKitFactory.h"
#include "Engine/DrumsetExporter.h"
#include "Engine/Preset.h"
#include <juce_core/juce_core.h>

using namespace dsl;

namespace
{

void addSample(Kit& kit, const juce::String& channelId, const juce::String& sourcePath)
{
    auto* snare = kit.findInstrumentById("snare");
    RoundRobinSlot slot;
    slot.channelSamples[channelId].sourcePath = sourcePath;
    snare->primaryArticulation()->velocityLayers.front().slots.push_back(std::move(slot));
}

juce::String firstExportedSamplePath(const Kit& kit)
{
    const auto* snare = kit.findInstrumentById("snare");
    return snare->primaryArticulation()->velocityLayers.front().slots.front()
        .channelSamples.begin()->second.sourcePath;
}

} // namespace

class DrumsetExporterTests : public juce::UnitTest
{
public:
    DrumsetExporterTests() : juce::UnitTest("Drumset export", "Engine") {}

    void runTest() override
    {
        const auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("swarm-drumset-export-tests", {}, false);
        expect(tempRoot.createDirectory());

        const auto sourceRoot = tempRoot.getChildFile("Original Samples");
        const auto firstSource = sourceRoot.getChildFile("A/shared name.wav");
        const auto secondSource = sourceRoot.getChildFile("B/shared name.wav");
        expect(firstSource.getParentDirectory().createDirectory());
        expect(secondSource.getParentDirectory().createDirectory());
        expect(firstSource.replaceWithText("first-v1"));
        expect(secondSource.replaceWithText("second-v1"));

        beginTest("Exports a portable preset and organized sample hierarchy");
        auto kit = DefaultKitFactory::createDefaultKit();
        kit.kitName = "Portable Test Kit";
        addSample(kit, "Top", "A/shared name.wav");
        addSample(kit, "Bottom", secondSource.getFullPathName());

        const auto package = tempRoot.getChildFile("Portable Test Kit_Swarm_Drumset");
        const auto exported = DrumsetExporter::exportKit(kit, package, sourceRoot, false);
        expect(exported.success, exported.errorMessage);
        expectEquals(exported.sampleReferences, 2);
        expectEquals(exported.filesCopied, 2);
        expect(package.getChildFile(DrumsetExporter::presetFileName).existsAsFile());
        expect(package.getChildFile(DrumsetExporter::manifestFileName).existsAsFile());
        expect(DrumsetExporter::isDrumsetExportDirectory(package));

        auto portableKit = Preset::fromJsonString(
            package.getChildFile(DrumsetExporter::presetFileName).loadFileAsString());
        const auto* portableSnare = portableKit.findInstrumentById("snare");
        expect(portableSnare != nullptr);
        for (auto& slot : portableSnare->primaryArticulation()->velocityLayers.front().slots)
            for (auto& [channelId, sample] : slot.channelSamples)
            {
                juce::ignoreUnused(channelId);
                expect(! juce::File::isAbsolutePath(sample.sourcePath));
                expect(sample.sourcePath.startsWith("Samples/"));
                expect(package.getChildFile(sample.sourcePath).existsAsFile());
            }

        // Exporting must not rewrite the live kit's source references.
        expectEquals(kit.findInstrumentById("snare")->primaryArticulation()
                         ->velocityLayers.front().slots.front().channelSamples.at("Top").sourcePath,
                     juce::String("A/shared name.wav"));

        beginTest("Refreshes managed samples and preset while preserving unknown files");
        expect(firstSource.replaceWithText("first-v2"));
        kit.masterDb = -4.5f;
        const auto userFile = package.getChildFile("Notes.txt");
        expect(userFile.replaceWithText("keep me"));

        const auto updated = DrumsetExporter::exportKit(kit, package, sourceRoot, true);
        expect(updated.success, updated.errorMessage);
        expect(updated.filesCopied >= 1);
        expect(userFile.existsAsFile());

        auto refreshedKit = Preset::fromJsonString(
            package.getChildFile(DrumsetExporter::presetFileName).loadFileAsString());
        expectWithinAbsoluteError(refreshedKit.masterDb, -4.5f, 0.0001f);
        const auto refreshedSample = package.getChildFile(firstExportedSamplePath(refreshedKit));
        expectEquals(refreshedSample.loadFileAsString(), juce::String("first-v2"));

        beginTest("Refuses incomplete exports when a referenced sample is missing");
        auto brokenKit = DefaultKitFactory::createDefaultKit();
        addSample(brokenKit, "Top", "does-not-exist.wav");
        const auto brokenPackage = tempRoot.getChildFile("Broken Export");
        const auto broken = DrumsetExporter::exportKit(brokenKit, brokenPackage, sourceRoot, false);
        expect(! broken.success);
        expectEquals(broken.missingSamplePaths.size(), 1);
        expect(! brokenPackage.getChildFile(DrumsetExporter::presetFileName).existsAsFile());

        beginTest("Update accepts only folders created by the drumset exporter");
        const auto arbitraryFolder = tempRoot.getChildFile("Arbitrary");
        expect(arbitraryFolder.createDirectory());
        const auto invalidUpdate = DrumsetExporter::exportKit(kit, arbitraryFolder, sourceRoot, true);
        expect(! invalidUpdate.success);

        expect(tempRoot.deleteRecursively());
    }
};

static DrumsetExporterTests drumsetExporterTestsInstance;
