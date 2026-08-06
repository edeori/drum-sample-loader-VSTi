#pragma once

#include "Kit.h"
#include <juce_core/juce_core.h>

namespace dsl
{

/** Creates and refreshes self-contained, portable drumset directories.
    The exported preset uses paths relative to preset.json and every referenced
    sample is copied into a deterministic instrument/articulation/layer/slot
    hierarchy below Samples/. */
class DrumsetExporter
{
public:
    struct Result
    {
        bool success = false;
        int sampleReferences = 0;
        int filesCopied = 0;
        int filesUnchanged = 0;
        juce::File packageDirectory;
        juce::String errorMessage;
        juce::StringArray missingSamplePaths;
    };

    static constexpr const char* presetFileName = "preset.json";
    static constexpr const char* manifestFileName = "swarm-drumset.json";

    static Result exportKit(const Kit& kit,
                            const juce::File& packageDirectory,
                            const juce::File& sourceBaseDirectory,
                            bool updateExistingExport);

    static bool isDrumsetExportDirectory(const juce::File& directory);
    static juce::String suggestedDirectoryName(const juce::String& kitName);

    /** Sanitizes a user-typed export name into a safe folder name, without the
        "_Swarm_Drumset" suffix that suggestedDirectoryName() appends for the
        auto-generated default. */
    static juce::String sanitizeExportName(const juce::String& desiredName);
};

} // namespace dsl
