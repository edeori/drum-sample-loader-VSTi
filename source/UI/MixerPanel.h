#pragma once

#include "DrumVaultLookAndFeel.h"
#include "KitEditController.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace dsl::ui
{

/** One vertical fader for a single mic channel of the currently selected
    instrument — pure balance control (docs/04-mixing-routing.md, section 4.1).
    Output routing lives one level up, in the Global Mixer (Global Settings
    view): overhead/room mics are physically shared across the whole kit, so
    routing them per instrument doesn't make sense, and grouping all routing
    in one place matches how a real multi-out instrument is patched in a DAW.
*/
class MixerChannelStrip : public juce::Component
{
public:
    MixerChannelStrip(KitEditController& controllerToUse, juce::String micChannelIdToUse);

    void paint(juce::Graphics&) override;
    void resized() override;

    /** Wires this strip to its OH_L/OH_R (or Room_L/Room_R) sibling: while
        *linkedFlag is true, dragging either fader mirrors the dB delta onto
        the other. Both strips of a pair must be linked to each other. */
    void setLinkPartner(MixerChannelStrip* partner, bool* linkedFlag) { linkPartner = partner; linkedFlagPtr = linkedFlag; }

    /** Applies a dB delta pushed by a linked partner's drag, without
        re-triggering this strip's own onValueChange (and so without
        re-mirroring back). */
    void applyLinkedDelta(double deltaDb);

private:
    KitEditController& controller;
    juce::String micChannelId;

    juce::Label nameLabel;
    ResettableSlider fader;
    juce::Label panLabel;
    juce::Label valueLabel;

    MixerChannelStrip* linkPartner = nullptr;
    bool* linkedFlagPtr = nullptr;
    double lastFaderValue = 0.0;
};

/** CHANNEL MIX panel: one fader per mic channel of the selected instrument. */
class MixerPanel : public juce::Component, private KitEditController::Listener
{
public:
    explicit MixerPanel(KitEditController& controllerToUse);
    ~MixerPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void kitEditorStateChanged() override;
    void rebuild();

    KitEditController& controller;
    juce::Label heading, statusLabel;
    juce::OwnedArray<MixerChannelStrip> strips;

    // One small link/unlink toggle per adjacent mono L/R ambience pair found
    // in rebuild(); linkButtonAfterIndex[i] is the strips[] index the button
    // straddles (sits on the boundary between that strip and the next).
    juce::OwnedArray<juce::TextButton> linkButtons;
    std::vector<int> linkButtonAfterIndex;
};

} // namespace dsl::ui
