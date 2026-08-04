#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace dsl
{

struct BusDef
{
    juce::String id;   // stable id referenced by Instrument::channelRouting, e.g. "bus:SnareTop"
    juce::String name; // UI label, e.g. "Snare Top"
    int numChannels = 1; // 1 = mono, 2 = stereo
};

/** Defines the plugin's multi-out bus layout (docs/01-architecture.md, section
    1.4). Bus order here must match the order the buses are added to the
    AudioProcessor in PluginProcessor, since routing resolves buses by index.
*/
class BusRoutingTable
{
public:
    std::vector<BusDef> buses;

    int indexOfBus(const juce::String& busId) const
    {
        for (int i = 0; i < (int) buses.size(); ++i)
            if (buses[(size_t) i].id == busId)
                return i;

        return -1;
    }

    static BusRoutingTable createDefault();
};

} // namespace dsl
