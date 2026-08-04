#pragma once

#include <juce_core/juce_core.h>

namespace dsl::ui
{

// Cubase/Steinberg-style octave numbering (MIDI note 60 = "C3"), matching the
// note names already used in docs/05-midi-mapping.md (e.g. note 42 = "F#1").
inline juce::String midiNoteToName(int note)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    if (note < 0 || note > 127)
        return "-";

    const int octave = note / 12 - 2;
    return juce::String(names[note % 12]) + juce::String(octave);
}

} // namespace dsl::ui
