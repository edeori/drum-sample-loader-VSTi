#pragma once

#include "Kit.h"

namespace dsl
{

/** Builds the default kit (Kick, Snare, Tom 1-4, Hi-hat, Crash 1-2,
    Ride, China, Splash) with its mic-channel templates, default routing, and
    the placeholder "Krimh-style" MIDI note table — see
    docs/02-instrument-model.md and docs/05-midi-mapping.md.

    All velocity layers are created empty (no round-robin samples assigned
    yet); the caller loads a real preset on top of this, or the user fills
    samples in via the editor.
*/
/** Whether an instrument's overhead/room ambience is captured as a discrete
    mono L/R pair (two independently faded/routed mic channels — the
    traditional default) or as a single stereo channel (one fader, one route,
    backed by one interleaved-stereo sample per round-robin slot). Some
    sessions only have a stereo-bounced overhead/room pair rather than
    discrete left/right multitrack files, so both are first-class options.
*/
enum class AmbienceFormat
{
    MonoPair,
    Stereo
};

class DefaultKitFactory
{
public:
    static Kit createDefaultKit();

    /** Upgrades older/custom kits so known Hi-hat, Crash, China and Ride
        instruments expose their required independently mapped articulations.
        Existing Main sample grids are retained and explicitly named Choke or
        Bell slots are moved to their matching articulation. */
    static void ensureRequiredArticulations(Kit& kit);

    /** Builds a single blank instrument (no samples) using the standard
        Body/Cymbal mic-channel template, for the "Add Instrument" UI flow
        (docs/02-instrument-model.md, section 2.4).
    */
    static Instrument createBlankInstrument(juce::String id, juce::String name,
                                             InstrumentCategory category, int midiNote,
                                             AmbienceFormat overheadFormat = AmbienceFormat::MonoPair,
                                             AmbienceFormat roomFormat = AmbienceFormat::MonoPair);

    /** Changes one existing instrument between discrete mono L/R ambience and
        one interleaved stereo channel. Sample mappings belonging to the other
        topology are retained, so switching back restores them. */
    static bool reconfigureAmbienceChannels(Instrument& instrument, MicRole role, AmbienceFormat format);
};

} // namespace dsl
