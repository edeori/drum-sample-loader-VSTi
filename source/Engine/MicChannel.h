#pragma once

#include <juce_core/juce_core.h>

namespace dsl
{

/** What role a mic channel plays acoustically. Determines whether the global
    Master Overhead/Room fader group applies to it (docs/04-mixing-routing.md,
    section 4.3). Channel *count* (mono vs stereo) is independent of role —
    see MicChannelDef::numChannels.
*/
enum class MicRole
{
    Spot,
    Overhead,
    Room
};

struct MicChannelDef
{
    juce::String id;          // stable id, e.g. "Top", "OH_L", or "OH" for a stereo pair — used as map key everywhere
    juce::String displayName; // UI label, e.g. "Top (Spot)"
    MicRole role = MicRole::Spot;

    // 1 = a standalone mono mic (e.g. two separate entries "OH_L"/"OH_R" for
    // a discrete stereo pair); 2 = a single stereo mic channel backed by one
    // interleaved-stereo sample and one fader/route (docs/02-instrument-model.md).
    int numChannels = 1;

    // -1 = hard left, 0 = centre/preserve stereo, +1 = hard right. Separate
    // OH/Room mono pairs are hard-panned by the default factory.
    float pan = 0.0f;
};

inline bool isOverheadRole(MicRole role) noexcept
{
    return role == MicRole::Overhead;
}

inline bool isRoomRole(MicRole role) noexcept
{
    return role == MicRole::Room;
}

inline bool isAmbienceRole(MicRole role) noexcept
{
    return isOverheadRole(role) || isRoomRole(role);
}

} // namespace dsl
