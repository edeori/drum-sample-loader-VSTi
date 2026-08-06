#pragma once

#include "MicChannel.h"
#include "RoundRobinSlot.h"
#include <map>
#include <vector>

namespace dsl
{

enum class InstrumentCategory
{
    Body,   // kick, snare, toms: spot mic(s) + shared overhead/room
    Cymbal  // hi-hat, crash, ride, china, splash: optional spot + overhead/room
};

enum class ArticulationAction
{
    Trigger,
    Choke
};

/** One independently MIDI-triggered playing style of an instrument. Each
    articulation owns its own velocity layers and round-robin samples, while
    sharing the parent instrument's mic layout, faders and routing. */
struct InstrumentArticulation
{
    juce::String id { "main" };
    juce::String name { "Main" };
    int midiNote = -1;
    ArticulationAction action = ArticulationAction::Trigger;

    std::vector<VelocityLayer> velocityLayers;
    std::vector<RoundRobinSlot> orphanedSlots;
};

/** One drum-kit piece: its mic channels, articulation-specific velocity and
    round-robin sample pools, per-channel fader/routing, and MIDI triggers.
    See docs/02-instrument-model.md for the full data model rationale.
*/
class Instrument
{
public:
    juce::String id;   // stable, preset-compatible identifier (e.g. "snare")
    juce::String name; // user-facing, renameable (e.g. "Snare")
    InstrumentCategory category = InstrumentCategory::Body;
    bool muted = false;
    bool solo = false;

    // Every instrument has at least one articulation. Simple instruments use
    // one "Main" entry; Hi-hat, Crash, China and Ride pieces expose multiple
    // MIDI notes.
    std::vector<InstrumentArticulation> articulations;

    std::vector<MicChannelDef> micChannels;

    std::map<juce::String, float> channelFaderDb;      // micChannel id -> gain in dB
    std::map<juce::String, juce::String> channelRouting; // micChannel id -> output bus id
    std::map<juce::String, bool> channelMuted;         // micChannel id -> muted (Global Mixer)
    std::map<juce::String, bool> channelSolo;          // micChannel id -> soloed (Global Mixer)

    // micChannel id -> L/R swapped (Global Mixer). Applied at trigger time in
    // DrumEngine against whatever's actually loaded for that hit: a mono
    // sample gets its pan mirrored, a genuinely stereo sample gets its two
    // channels swapped outright — see DrumEngine::triggerNoteOn.
    std::map<juce::String, bool> channelSwapLR;

    // Kit Editor Channel Mix: when linked, dragging one of the OH_L/OH_R (or
    // Room_L/Room_R) mono fader pair mirrors the dB delta onto its sibling.
    // Only meaningful for a mono L/R pair (Stereo-format ambience has one
    // shared fader already). Defaults linked — moving them together is the
    // common case.
    bool ohFaderLinked = true;
    bool roomFaderLinked = true;

    MicChannelDef* findChannel(const juce::String& micChannelId);
    const MicChannelDef* findChannel(const juce::String& micChannelId) const;
    float getChannelFaderDb(const juce::String& micChannelId) const;
    juce::String getChannelRouting(const juce::String& micChannelId) const;
    bool isChannelMuted(const juce::String& micChannelId) const;
    bool isChannelSoloed(const juce::String& micChannelId) const;
    bool isChannelSwapped(const juce::String& micChannelId) const;
    InstrumentArticulation* findArticulation(const juce::String& articulationId);
    const InstrumentArticulation* findArticulation(const juce::String& articulationId) const;
    InstrumentArticulation* primaryArticulation();
    const InstrumentArticulation* primaryArticulation() const;
};

} // namespace dsl
