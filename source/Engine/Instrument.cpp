#include "Instrument.h"

namespace dsl
{

MicChannelDef* Instrument::findChannel(const juce::String& micChannelId)
{
    for (auto& ch : micChannels)
        if (ch.id == micChannelId)
            return &ch;

    return nullptr;
}

const MicChannelDef* Instrument::findChannel(const juce::String& micChannelId) const
{
    for (auto& ch : micChannels)
        if (ch.id == micChannelId)
            return &ch;

    return nullptr;
}

float Instrument::getChannelFaderDb(const juce::String& micChannelId) const
{
    auto it = channelFaderDb.find(micChannelId);
    return it == channelFaderDb.end() ? 0.0f : it->second;
}

juce::String Instrument::getChannelRouting(const juce::String& micChannelId) const
{
    auto it = channelRouting.find(micChannelId);
    return it == channelRouting.end() ? juce::String() : it->second;
}

bool Instrument::isChannelMuted(const juce::String& micChannelId) const
{
    auto it = channelMuted.find(micChannelId);
    return it != channelMuted.end() && it->second;
}

bool Instrument::isChannelSoloed(const juce::String& micChannelId) const
{
    auto it = channelSolo.find(micChannelId);
    return it != channelSolo.end() && it->second;
}

bool Instrument::isChannelSwapped(const juce::String& micChannelId) const
{
    auto it = channelSwapLR.find(micChannelId);
    return it != channelSwapLR.end() && it->second;
}

InstrumentArticulation* Instrument::findArticulation(const juce::String& articulationId)
{
    for (auto& articulation : articulations)
        if (articulation.id == articulationId)
            return &articulation;

    return nullptr;
}

const InstrumentArticulation* Instrument::findArticulation(const juce::String& articulationId) const
{
    for (auto& articulation : articulations)
        if (articulation.id == articulationId)
            return &articulation;

    return nullptr;
}

InstrumentArticulation* Instrument::primaryArticulation()
{
    return articulations.empty() ? nullptr : &articulations.front();
}

const InstrumentArticulation* Instrument::primaryArticulation() const
{
    return articulations.empty() ? nullptr : &articulations.front();
}

} // namespace dsl
