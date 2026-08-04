#pragma once

#include "DrumVaultLookAndFeel.h"
#include "InstrumentArt.h"
#include "KitEditController.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

namespace dsl::ui
{

/** One clickable spot on the kit stage:
      - Hotspot: an invisible region overlaid on the drumset photo (the normal
        case for the default pieces) — just draws a glow/selection marker.
      - StandaloneTile: any custom-added instrument (cymbal or body), which
        has no fixed position on the photo — a small self-contained tile in
        the bottom overflow strip, so nothing is ever unreachable from this view.

    Glows briefly whenever its MIDI note is actually hit (host playback, a
    live controller, or the click itself), read from
    DrumEngine::getLastHitTimeMs — real-time-safe, audio-thread-written,
    UI-thread-polled.
*/
class HotspotPad : public juce::Component
{
public:
    enum class DisplayMode { Hotspot, StandaloneTile };

    HotspotPad(KitEditController& controllerToUse, juce::String instrumentIdToUse,
               juce::String articulationIdToUse = {}, juce::String hotspotIdToUse = {});

    const juce::String& getInstrumentId() const { return instrumentId; }
    const juce::String& getHotspotId() const { return hotspotId; }
    void setDisplayMode(DisplayMode mode);

    void paint(juce::Graphics&) override;
    bool hitTest(int x, int y) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;

private:
    KitEditController& controller;
    juce::String instrumentId;
    juce::String articulationId;
    juce::String hotspotId;
    juce::Image art;
    DisplayMode displayMode = DisplayMode::Hotspot;
};

/** KIT STAGE: an interactive, "hittable" view of the whole kit, built on the
    drumset-only.png photo — click a drum or cymbal to select + audition it,
    and watch it glow when MIDI actually strikes it.
*/
class KitStageView : public juce::Component, private KitEditController::Listener, private juce::Timer
{
public:
    explicit KitStageView(KitEditController& controllerToUse);
    ~KitStageView() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void kitEditorStateChanged() override { rebuildPads(); }
    void timerCallback() override { repaint(); } // drives every HotspotPad's hit-glow decay
    void rebuildPads();
    void layoutPads();

    static const std::map<juce::String, juce::Rectangle<float>>& defaultHotspots();

    KitEditController& controller;
    juce::Label heading, hintLabel;
    juce::Image stageImage;
    juce::Image disabledStageImage;
    juce::Rectangle<int> stageArea;
    juce::Rectangle<int> imageBounds;
    juce::OwnedArray<HotspotPad> pads;
};

} // namespace dsl::ui
