#include "BusRoutingTable.h"

namespace dsl
{

BusRoutingTable BusRoutingTable::createDefault()
{
    BusRoutingTable table;
    table.buses.push_back({ "bus:Master", "Master", 2 });

    // A generous, generic pool of stereo output pairs — mirrors how
    // Kontakt-style multi-out instruments present themselves to a host: the
    // DAW sees "Out 1/2", "Out 3/4", ... as separate assignable outputs that
    // the user patches into their own mixer, rather than a fixed set of
    // instrument-specific bus names. DefaultKitFactory picks sensible
    // defaults from this pool for the stock kit; any instrument's channel
    // (including user-added ones) can be routed to any of them
    // (docs/04-mixing-routing.md, section 4.2).
    constexpr int numGenericStereoPairs = 16;
    for (int i = 0; i < numGenericStereoPairs; ++i)
    {
        const int leftChannelNumber = i * 2 + 1;
        table.buses.push_back({ "bus:Out" + juce::String(i + 1),
                                 "Out " + juce::String(leftChannelNumber) + "/" + juce::String(leftChannelNumber + 1),
                                 2 });
    }

    return table;
}

} // namespace dsl
