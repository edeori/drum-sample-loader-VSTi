#pragma once

#include <juce_core/juce_core.h>

namespace dsl
{

/** Picks the next round-robin slot index, excluding the previously played one
    (docs/03-editor-ui.md, section 3.4). Pure function so it is trivially unit
    testable without any engine/audio state.

    @param numSlots     number of available slots (0 = nothing to play)
    @param previousSlot index last played, or -1 if none yet
    @param rng           source of randomness

    @returns -1 if numSlots == 0, otherwise a valid index in [0, numSlots).
             If numSlots == 1, always returns 0 (nothing else to alternate to).
*/
inline int selectRoundRobinSlot(int numSlots, int previousSlot, juce::Random& rng) noexcept
{
    if (numSlots <= 0)
        return -1;

    if (numSlots == 1)
        return 0;

    if (previousSlot < 0 || previousSlot >= numSlots)
        return rng.nextInt(numSlots); // no valid previous slot to exclude

    int chosen = rng.nextInt(numSlots - 1); // in [0, numSlots - 2]
    if (chosen >= previousSlot)
        ++chosen; // skip over previousSlot, remapping into [0, numSlots) \ {previousSlot}

    return chosen;
}

} // namespace dsl
