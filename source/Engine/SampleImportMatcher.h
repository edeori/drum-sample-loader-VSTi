#pragma once

#include "Instrument.h"
#include <optional>

namespace dsl
{

struct SampleImportMatch
{
    juce::String channelId;
    juce::String variationKey;
};

/** Resolves a sample filename to one mic channel and one round-robin
    variation. Handles both the original "{hit}-{mic}.wav" convention and
    common stereo ambience names such as OH.wav, Room.wav, Snare_OH_01.wav,
    and Snare Room Stereo 01.wav. */
class SampleImportMatcher
{
public:
    static std::optional<SampleImportMatch> match(const juce::String& fileNameWithoutExtension,
                                                   const Instrument& instrument);

    /** True for explicit stereo naming markers such as Stereo, ST, OH_ST,
        Overheads-ST and Room ST. */
    static bool hasStereoMarker(const juce::String& fileNameWithoutExtension);

    /** Recognizes both the full Choke label and the common (chk) shorthand. */
    static bool hasChokeMarker(const juce::String& fileNameWithoutExtension);

    /** True for a dedicated centre/mid Room mic suffix such as Room M. */
    static bool hasRoomMidMarker(const juce::String& fileNameWithoutExtension);

    /** Returns the destination articulation for a batch-imported file.
        Explicit Bell/Choke filename markers may split a mixed batch, while
        unmarked files always stay in the articulation selected by the user. */
    static juce::String targetArticulationId(const juce::String& fileNameWithoutExtension,
                                              const Instrument& instrument,
                                              const juce::String& selectedArticulationId);
};

} // namespace dsl
