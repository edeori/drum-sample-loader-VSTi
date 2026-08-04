#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace dsl
{

/** The single, kit-wide velocity-layer configuration (docs/03-editor-ui.md,
    section 3.2). There is exactly one of these per Kit; every instrument's
    VelocityLayer array is kept in sync with it (see Kit::setVelocityLayerCount).
*/
class GlobalVelocityConfig
{
public:
    GlobalVelocityConfig();

    /** Resizes layerNames/thresholds to match. Existing names/thresholds are
        preserved where possible; new layers get generated placeholder names.
        Does NOT touch any instrument data — that is Kit's responsibility.
    */
    void setLayerCount(int newCount);

    int getLayerCount() const noexcept { return (int) layerNames.size(); }

    /** Maps a MIDI velocity (0-127) to a layer index (0 = softest). */
    int selectLayerIndex(int velocity) const noexcept;

    /** Returns a representative note-on velocity from the centre of a layer's
        configured range. Useful for UI preview buttons. */
    int previewVelocityForLayer(int layerIndex) const noexcept;

    std::vector<juce::String> layerNames;
    std::vector<int> thresholds; // ascending, size == layerNames.size() - 1, each in [1, 126]

private:
    void renumberDefaultNames();
};

} // namespace dsl
