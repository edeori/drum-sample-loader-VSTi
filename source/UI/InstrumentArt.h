#pragma once

#include "../Engine/Instrument.h"
#include <BinaryData.h>
#include <juce_graphics/juce_graphics.h>

namespace dsl::ui
{

/** The engine model only distinguishes Body/Cymbal; the mockup's card art
    additionally distinguishes kick/snare/tom silhouettes. This infers the
    closest artwork from the instrument id/name, falling back sensibly for
    user-added instruments.
*/
inline const char* instrumentArtDataFor(const Instrument& inst, int& outSize)
{
    const auto id = inst.id.toLowerCase();

    if (inst.category == InstrumentCategory::Cymbal)
    {
        outSize = BinaryData::cymbal_pngSize;
        return BinaryData::cymbal_png;
    }

    if (id.contains("kick"))
    {
        outSize = BinaryData::kick_pngSize;
        return BinaryData::kick_png;
    }

    if (id.contains("snare"))
    {
        outSize = BinaryData::snare_pngSize;
        return BinaryData::snare_png;
    }

    outSize = BinaryData::tom_pngSize;
    return BinaryData::tom_png;
}

inline juce::Image loadInstrumentArt(const Instrument& inst)
{
    int size = 0;
    const char* data = instrumentArtDataFor(inst, size);
    return juce::ImageCache::getFromMemory(data, size);
}

} // namespace dsl::ui
