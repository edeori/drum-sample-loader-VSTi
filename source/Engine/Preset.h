#pragma once

#include "Kit.h"
#include <juce_core/juce_core.h>

namespace dsl
{

/** Converts a Kit to/from the JSON preset schema described in
    docs/06-preset-format.md. Pure (de)serialization only — no file IO, no
    sample decoding. Sample paths are round-tripped verbatim as
    SampleRef::sourcePath; resolving them (relative-to-preset -> absolute) and
    loading the audio itself is SamplePool's job, kept deliberately decoupled
    (docs/06-preset-format.md, section 6.3).
*/
class Preset
{
public:
    static juce::var toVar(const Kit& kit);
    static juce::String toJsonString(const Kit& kit, bool pretty = true);

    static Kit fromVar(const juce::var& json);
    static Kit fromJsonString(const juce::String& jsonText);
};

} // namespace dsl
