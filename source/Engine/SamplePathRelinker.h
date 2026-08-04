#pragma once

#include "Kit.h"
#include <juce_core/juce_core.h>

namespace dsl
{

/** Repairs missing preset sample paths by looking for the same files below a
    user-selected directory. Matching is deliberately conservative: an exact
    filename is required and ambiguous best matches are left unresolved. */
class SamplePathRelinker
{
public:
    struct Result
    {
        int relinked = 0;
        int ambiguous = 0;
        juce::StringArray unresolvedPaths;
    };

    static Result relinkMissingSamples(Kit& kit,
                                       const juce::File& presetDirectory,
                                       const juce::File& searchDirectory);
};

} // namespace dsl
