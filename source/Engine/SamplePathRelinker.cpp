#include "SamplePathRelinker.h"
#include <map>

namespace dsl
{
namespace
{

bool isSupportedAudioFile(const juce::File& file)
{
    const auto extension = file.getFileExtension().toLowerCase();
    return extension == ".wav" || extension == ".aif" || extension == ".aiff";
}

juce::StringArray pathComponents(const juce::String& path)
{
    auto normalised = path.replaceCharacter('\\', '/');
    juce::StringArray components;
    components.addTokens(normalised, "/", {});
    components.removeEmptyStrings();
    return components;
}

int matchingParentSuffixLength(const juce::String& originalPath, const juce::File& candidate)
{
    auto original = pathComponents(originalPath);
    auto replacement = pathComponents(candidate.getFullPathName());

    // The filename is already known to match. Score equal parent directory
    // names from right to left so moving or renaming the volume/root still
    // allows an otherwise intact sample-pack hierarchy to be recovered.
    int originalIndex = original.size() - 2;
    int replacementIndex = replacement.size() - 2;
    int score = 0;
    while (originalIndex >= 0 && replacementIndex >= 0
           && original[originalIndex].equalsIgnoreCase(replacement[replacementIndex]))
    {
        ++score;
        --originalIndex;
        --replacementIndex;
    }
    return score;
}

juce::File resolveStoredPath(const juce::String& sourcePath, const juce::File& presetDirectory)
{
    return juce::File::isAbsolutePath(sourcePath)
        ? juce::File(sourcePath)
        : presetDirectory.getChildFile(sourcePath);
}

} // namespace

SamplePathRelinker::Result SamplePathRelinker::relinkMissingSamples(
    Kit& kit, const juce::File& presetDirectory, const juce::File& searchDirectory)
{
    Result result;
    if (! searchDirectory.isDirectory())
        return result;

    juce::Array<juce::File> files;
    searchDirectory.findChildFiles(files, juce::File::findFiles, true);

    std::map<juce::String, std::vector<juce::File>> filesByLowercaseName;
    for (const auto& file : files)
        if (isSupportedAudioFile(file))
            filesByLowercaseName[file.getFileName().toLowerCase()].push_back(file);

    for (auto& instrument : kit.instruments)
        for (auto& articulation : instrument.articulations)
            for (auto& layer : articulation.velocityLayers)
                for (auto& slot : layer.slots)
                    for (auto& [channelId, sample] : slot.channelSamples)
                    {
                        juce::ignoreUnused(channelId);
                        if (sample.sourcePath.isEmpty()
                            || resolveStoredPath(sample.sourcePath, presetDirectory).existsAsFile())
                            continue;

                        const auto key = juce::File(sample.sourcePath).getFileName().toLowerCase();
                        const auto found = filesByLowercaseName.find(key);
                        if (found == filesByLowercaseName.end() || found->second.empty())
                        {
                            result.unresolvedPaths.addIfNotAlreadyThere(sample.sourcePath);
                            continue;
                        }

                        const auto& candidates = found->second;
                        const juce::File* best = &candidates.front();
                        int bestScore = matchingParentSuffixLength(sample.sourcePath, *best);
                        int numBest = 1;

                        for (size_t i = 1; i < candidates.size(); ++i)
                        {
                            const int score = matchingParentSuffixLength(sample.sourcePath, candidates[i]);
                            if (score > bestScore)
                            {
                                best = &candidates[i];
                                bestScore = score;
                                numBest = 1;
                            }
                            else if (score == bestScore)
                            {
                                ++numBest;
                            }
                        }

                        if (numBest != 1)
                        {
                            ++result.ambiguous;
                            result.unresolvedPaths.addIfNotAlreadyThere(sample.sourcePath);
                            continue;
                        }

                        sample.sourcePath = best->getFullPathName();
                        ++result.relinked;
                    }

    return result;
}

} // namespace dsl
