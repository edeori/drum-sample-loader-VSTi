#pragma once

#include "DrumVaultLookAndFeel.h"
#include "KitEditController.h"
#include <juce_gui_basics/juce_gui_basics.h>

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

private:
    KitEditController& controller;
    juce::String micChannelId;

    juce::Label nameLabel;
    ResettableSlider fader;
    juce::Label panLabel;
    juce::Label valueLabel;
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
};

} // namespace dsl::ui
