#pragma once

#include "BusRoutingTable.h"
#include "GlobalVelocityConfig.h"
#include "Instrument.h"
#include <vector>

namespace dsl
{

/** The full, self-contained state of one drum kit: instruments, the shared
    velocity-layer config, bus routing, and the three master faders
    (docs/04-mixing-routing.md, docs/06-preset-format.md).
*/
class Kit
{
public:
    juce::String kitName { "Untitled Kit" };

    GlobalVelocityConfig velocityConfig;
    BusRoutingTable busRouting = BusRoutingTable::createDefault();
    std::vector<Instrument> instruments;

    float masterDb = 0.0f;
    float masterOverheadDb = 0.0f;
    float masterRoomDb = 0.0f;
    float masterTuneSemitones = 0.0f;

    // Every instrument's Overhead/Room mic channels share ONE kit-wide output
    // destination each — the overheads/room mics physically capture the whole
    // kit, so routing them per-instrument would be incoherent. Only Spot
    // channels (Instrument::channelRouting) are routed per instrument. Set
    // from the Global Mixer, not per-instrument (docs/04-mixing-routing.md).
    juce::String overheadBusId { "bus:Master" };
    juce::String roomBusId { "bus:Master" };

    // Pooled Overhead/Room mute+solo, set from the Global Mixer alongside the
    // per-instrument Spot channel mute+solo below — same kit-wide-destination
    // reasoning as overheadBusId/roomBusId above.
    bool overheadMuted = false;
    bool overheadSolo = false;
    bool roomMuted = false;
    bool roomSolo = false;

    bool hasAnySoloedInstrument() const
    {
        for (auto& instrument : instruments)
            if (instrument.solo)
                return true;

        return false;
    }

    // Global Mixer channel-level solo (independent of the Kit Editor's
    // per-instrument solo above): true if ANY individual mixer channel —
    // a Spot channel on any instrument, or the pooled Overheads/Room rows —
    // is currently soloed.
    bool hasAnySoloedChannel() const
    {
        if (overheadSolo || roomSolo)
            return true;

        for (auto& instrument : instruments)
            for (auto& [chId, soloed] : instrument.channelSolo)
                if (soloed)
                    return true;

        return false;
    }

    Instrument* findInstrumentById(const juce::String& instrumentId);
    const Instrument* findInstrumentById(const juce::String& instrumentId) const;

    Instrument* instrumentForMidiNote(int midiNote);

    struct MidiTarget
    {
        Instrument* instrument = nullptr;
        InstrumentArticulation* articulation = nullptr;
    };

    MidiTarget targetForMidiNote(int midiNote);

    enum class SetNoteResult
    {
        Ok,
        Conflict // another instrument already owns that note; nothing changed
    };

    /** Assigns midiNote to the given instrument. If another instrument already
        owns that note: returns Conflict and changes nothing unless force is
        true, in which case the other instrument's note is cleared (set to -1)
        and the assignment proceeds (docs/05-midi-mapping.md, section 5.1).
    */
    SetNoteResult setInstrumentMidiNote(const juce::String& instrumentId, int midiNote, bool force);
    SetNoteResult setArticulationMidiNote(const juce::String& instrumentId,
                                          const juce::String& articulationId,
                                          int midiNote, bool force);

    /** Resizes every articulation's velocityLayers to match newCount,
        preserving existing layers by index. Layers dropped by a shrink are
        appended to that articulation's orphanedSlots rather than deleted
        (docs/03-editor-ui.md, section 3.2). Also updates velocityConfig itself.
    */
    void setVelocityLayerCount(int newCount);

private:
    static void resizeArticulationVelocityLayers(InstrumentArticulation& articulation, int newCount);
};

} // namespace dsl
