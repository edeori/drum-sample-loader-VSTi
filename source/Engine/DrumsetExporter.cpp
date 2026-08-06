#include "DrumsetExporter.h"
#include "Preset.h"

namespace dsl
{
namespace
{

juce::String safePathComponent(juce::String value, const juce::String& fallback)
{
    value = value.trim();
    juce::String safe;
    bool previousWasSeparator = false;

    for (auto c : value)
    {
        const bool allowed = juce::CharacterFunctions::isLetterOrDigit(c) || c == '-' || c == '_';
        if (allowed)
        {
            safe += juce::String::charToString(c);
            previousWasSeparator = false;
        }
        else if (! previousWasSeparator && safe.isNotEmpty())
        {
            safe += "_";
            previousWasSeparator = true;
        }
    }

    safe = safe.trimCharactersAtEnd("_.");
    return safe.isNotEmpty() ? safe : fallback;
}

juce::String twoDigitIndex(int zeroBased)
{
    return juce::String(zeroBased + 1).paddedLeft('0', 2);
}

juce::String threeDigitIndex(int zeroBased)
{
    return juce::String(zeroBased + 1).paddedLeft('0', 3);
}

juce::File resolveSourceFile(const juce::String& storedPath, const juce::File& sourceBaseDirectory)
{
    if (juce::File::isAbsolutePath(storedPath))
        return juce::File(storedPath);

    return sourceBaseDirectory.isDirectory()
        ? sourceBaseDirectory.getChildFile(storedPath)
        : juce::File(storedPath);
}

juce::String safeExportFileName(const juce::String& channelId, const juce::File& source)
{
    const auto extension = source.getFileExtension();
    const auto baseName = safePathComponent(source.getFileNameWithoutExtension(), "sample");
    return safePathComponent(channelId, "channel") + "--" + baseName + extension.toLowerCase();
}

} // namespace

bool DrumsetExporter::isDrumsetExportDirectory(const juce::File& directory)
{
    if (! directory.isDirectory())
        return false;

    const auto manifest = juce::JSON::parse(directory.getChildFile(manifestFileName));
    return manifest.getProperty("type", {}).toString() == "SwarmDrumsetExport"
        && (int) manifest.getProperty("packageVersion", 0) == 1;
}

juce::String DrumsetExporter::suggestedDirectoryName(const juce::String& kitName)
{
    return safePathComponent(kitName, "Untitled_Kit") + "_Swarm_Drumset";
}

juce::String DrumsetExporter::sanitizeExportName(const juce::String& desiredName)
{
    return safePathComponent(desiredName, "Untitled_Kit");
}

DrumsetExporter::Result DrumsetExporter::exportKit(
    const Kit& kit,
    const juce::File& packageDirectory,
    const juce::File& sourceBaseDirectory,
    bool updateExistingExport)
{
    Result result;
    result.packageDirectory = packageDirectory;

    if (packageDirectory == juce::File {})
    {
        result.errorMessage = "No export directory was selected.";
        return result;
    }

    if (updateExistingExport && ! isDrumsetExportDirectory(packageDirectory))
    {
        result.errorMessage = "The selected folder is not a valid Swarm drumset export.";
        return result;
    }

    if (! updateExistingExport && packageDirectory.exists())
    {
        juce::Array<juce::File> existingFiles;
        packageDirectory.findChildFiles(existingFiles, juce::File::findFilesAndDirectories, false);
        if (! existingFiles.isEmpty())
        {
            result.errorMessage = "The new export folder is not empty.";
            return result;
        }
    }

    Kit exportedKit = kit;

    struct PlannedCopy
    {
        juce::File source;
        juce::File destination;
        juce::String relativePath;
        SampleRef* exportedReference = nullptr;
    };
    std::vector<PlannedCopy> copies;

    for (int instrumentIndex = 0; instrumentIndex < (int) exportedKit.instruments.size(); ++instrumentIndex)
    {
        auto& instrument = exportedKit.instruments[(size_t) instrumentIndex];
        const auto instrumentFolder = twoDigitIndex(instrumentIndex) + "-"
            + safePathComponent(instrument.id.isNotEmpty() ? instrument.id : instrument.name, "instrument");

        for (auto& articulation : instrument.articulations)
        {
            const auto articulationFolder = safePathComponent(
                articulation.id.isNotEmpty() ? articulation.id : articulation.name, "main");

            for (int layerIndex = 0; layerIndex < (int) articulation.velocityLayers.size(); ++layerIndex)
            {
                auto& layer = articulation.velocityLayers[(size_t) layerIndex];
                for (int slotIndex = 0; slotIndex < (int) layer.slots.size(); ++slotIndex)
                {
                    auto& slot = layer.slots[(size_t) slotIndex];
                    for (auto& [channelId, sample] : slot.channelSamples)
                    {
                        if (sample.sourcePath.isEmpty())
                            continue;

                        const auto source = resolveSourceFile(sample.sourcePath, sourceBaseDirectory);
                        if (! source.existsAsFile())
                        {
                            result.missingSamplePaths.addIfNotAlreadyThere(sample.sourcePath);
                            continue;
                        }

                        const auto destinationDirectory = packageDirectory.getChildFile("Samples")
                            .getChildFile(instrumentFolder)
                            .getChildFile(articulationFolder)
                            .getChildFile("Layer-" + twoDigitIndex(layerIndex))
                            .getChildFile("Slot-" + threeDigitIndex(slotIndex));
                        const auto destination = destinationDirectory.getChildFile(
                            safeExportFileName(channelId, source));
                        auto relativePath = destination.getRelativePathFrom(packageDirectory)
                            .replaceCharacter('\\', '/');

                        copies.push_back({ source, destination, relativePath, &sample });
                    }
                }
            }
        }
    }

    result.sampleReferences = (int) copies.size() + result.missingSamplePaths.size();
    if (! result.missingSamplePaths.isEmpty())
    {
        result.errorMessage = juce::String(result.missingSamplePaths.size())
            + " referenced sample file(s) could not be found. Nothing was exported.";
        return result;
    }

    if (! packageDirectory.createDirectory())
    {
        result.errorMessage = "The export directory could not be created.";
        return result;
    }

    juce::StringArray managedFiles;
    for (auto& copy : copies)
    {
        if (! copy.destination.getParentDirectory().createDirectory())
        {
            result.errorMessage = "Could not create sample subfolder: "
                + copy.destination.getParentDirectory().getFullPathName();
            return result;
        }

        const bool sameFile = copy.source == copy.destination;
        const bool identical = ! sameFile && copy.destination.existsAsFile()
            && copy.source.hasIdenticalContentTo(copy.destination);

        if (sameFile || identical)
        {
            ++result.filesUnchanged;
        }
        else if (copy.source.copyFileTo(copy.destination))
        {
            ++result.filesCopied;
        }
        else
        {
            result.errorMessage = "Could not copy sample: " + copy.source.getFullPathName();
            return result;
        }

        copy.exportedReference->sourcePath = copy.relativePath;
        managedFiles.addIfNotAlreadyThere(copy.relativePath);
    }

    const auto presetFile = packageDirectory.getChildFile(presetFileName);
    if (! presetFile.replaceWithText(Preset::toJsonString(exportedKit)))
    {
        result.errorMessage = "The portable preset could not be written.";
        return result;
    }

    auto manifestObject = std::make_unique<juce::DynamicObject>();
    manifestObject->setProperty("type", "SwarmDrumsetExport");
    manifestObject->setProperty("packageVersion", 1);
    manifestObject->setProperty("kitName", kit.kitName);
    manifestObject->setProperty("presetFile", presetFileName);
    manifestObject->setProperty("samplesDirectory", "Samples");
    manifestObject->setProperty("updatedAt", juce::Time::getCurrentTime().toISO8601(true));
    manifestObject->setProperty("sampleReferences", result.sampleReferences);
    juce::Array<juce::var> managedFileArray;
    for (auto& path : managedFiles)
        managedFileArray.add(path);
    manifestObject->setProperty("managedFiles", managedFileArray);

    const auto manifestFile = packageDirectory.getChildFile(manifestFileName);
    if (! manifestFile.replaceWithText(juce::JSON::toString(juce::var(manifestObject.release()), false)))
    {
        result.errorMessage = "The drumset export manifest could not be written.";
        return result;
    }

    result.success = true;
    return result;
}

} // namespace dsl
