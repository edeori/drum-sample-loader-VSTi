#pragma once

#include "Dialogs.h"
#include "DrumVaultLookAndFeel.h"
#include "KitEditController.h"
#include "ToastOverlay.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace dsl::ui
{

/** GLOBAL MIXER card: one vertical channel strip per instrument spot channel
    (Kick In/Out, Snare Top/Bottom, cymbal spots, ...), plus one pooled strip
    each for Overheads and Room — this is where output routing to the host's
    real multi-out buses actually happens (docs/04-mixing-routing.md, section
    4.2). Each strip is a fader with its output-bus routing directly below
    it, console-style. Per-instrument balance faders live in the Kit Editor's
    Channel Mix panel; this card is the single place all of that ultimately
    gets patched out.
*/
class GlobalMixerCard : public juce::Component, private KitEditController::Listener, private juce::Timer
{
public:
    explicit GlobalMixerCard(KitEditController& controllerToUse);
    ~GlobalMixerCard() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    int getPreferredWidth() const;

private:
    void kitEditorStateChanged() override { rebuildRows(); }
    void timerCallback() override { repaint(); } // drives the per-channel output meters
    void rebuildRows();

    struct Row
    {
        juce::Label nameLabel;
        ResettableSlider fader;
        juce::Label valueLabel;
        ResettableSlider pan;
        juce::Label panValueLabel;
        juce::ComboBox routeBox;
        juce::TextButton muteButton { "M" }, soloButton { "S" };
        // Slot strip between mute/solo and the fader — currently just the
        // L/R swap toggle (Overheads/Room rows only), but laid out as a
        // reserved row so future per-channel extras have somewhere to go.
        juce::TextButton swapButton { juce::String::charToString((juce::juce_wchar) 0x21c4) };
        bool hasSwapControl = false;
        juce::String currentBusId; // kept in sync with routeBox for meter polling
        uint64_t meterKey = 0;     // see DrumEngine::meterKeyFor
        bool hasPanControl = false;
        juce::Rectangle<int> scaleBounds, meterBounds, slotBounds;
    };

    KitEditController& controller;
    juce::OwnedArray<Row> rows;
    static constexpr int stripWidth = 132;
};

/** The plugin's overall output level — pinned to the right edge of the
    Global Mixer, outside the horizontally scrollable channel strips, so it
    stays visible (and reachable) regardless of scroll position. Mirrors
    MasterCard's volume row/MeterCard's peak reading, just surfaced where
    the channels it's summing are. */
class MasterStripPanel : public juce::Component, private KitEditController::Listener, private juce::Timer
{
public:
    explicit MasterStripPanel(KitEditController& controllerToUse);
    ~MasterStripPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    static constexpr int preferredWidth = 132;

private:
    void kitEditorStateChanged() override { refresh(); }
    void timerCallback() override { repaint(); } // drives the meter
    void refresh();

    KitEditController& controller;
    juce::Label nameLabel;
    ResettableSlider fader;
    juce::Label valueLabel;
    juce::Rectangle<int> scaleBounds, meterBounds;
};

/** GLOBAL MIXER: its own full view (a separate top-bar tab) built around a
    single GlobalMixerCard in a horizontally scrollable viewport — kept out
    of GLOBAL SETTINGS' longer card stack so it gets the whole window to
    itself. Scrolls sideways through channel strips rather than a vertical
    list, so it isn't affected by the vertical touchpad-scroll-vs-slider
    conflict a taller vertical layout would have. The Master strip sits
    outside the viewport, pinned to the right edge. */
class GlobalMixerView : public juce::Component
{
public:
    explicit GlobalMixerView(KitEditController& controllerToUse);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label heading, hintLabel;
    juce::Viewport viewport;
    GlobalMixerCard globalMixerCard;
    MasterStripPanel masterStripPanel;
};

/** VELOCITY THRESHOLDS card: layer count, the coloured range strip, and the
    per-threshold sliders (docs/03-editor-ui.md, section 3.2). */
class VelocityThresholdsCard : public juce::Component, private KitEditController::Listener
{
public:
    explicit VelocityThresholdsCard(KitEditController& controllerToUse);
    ~VelocityThresholdsCard() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    int getPreferredHeight() const;

private:
    void kitEditorStateChanged() override { refresh(); }
    void refresh();
    void rebuildThresholdRows();

    KitEditController& controller;
    juce::Label titleLabel;
    juce::TextButton resetButton { "RESET" };
    juce::Label layerCountFieldLabel;
    juce::ComboBox layerCountBox;
    juce::Rectangle<int> stripBounds;

    struct ThresholdRow
    {
        juce::Label nameLabel, valueLabel;
        juce::Slider slider;
    };
    juce::OwnedArray<ThresholdRow> rows;

    juce::Label infoText;
    juce::TextButton orphanedButton;
};

/** MASTER card: overall / overheads / room / tune (docs/04-mixing-routing.md
    section 4.3-4.4). */
class MasterCard : public juce::Component
{
public:
    explicit MasterCard(KitEditController& controllerToUse);

    void paint(juce::Graphics&) override;
    void resized() override;
    static constexpr int preferredHeight = 190;

private:
    struct Row { juce::Label label; ResettableSlider slider; juce::Label value; };
    Row volumeRow, overheadRow, roomRow, tuneRow;
    juce::Label titleLabel;
    juce::Label outputLabel;

    void setupRow(Row& row, const juce::String& name, double lo, double hi, double step, double initial,
                  std::function<void(float)> onChange, const juce::String& unit);
};

/** MASTER OUTPUT stereo peak meter, polling the processor at its own rate. */
class MeterCard : public juce::Component, private juce::Timer
{
public:
    explicit MeterCard(KitEditController& controllerToUse);

    void paint(juce::Graphics&) override;
    void resized() override;
    static constexpr int preferredHeight = 130;

private:
    void timerCallback() override;
    static float gainToDb(float g);

    KitEditController& controller;
    juce::Rectangle<int> meterLBounds, meterRBounds;
    float displayedL = 0.0f, displayedR = 0.0f;
    float peakDbL = -100.0f, peakDbR = -100.0f;
};

/** MIDI MAP card: read-only overview + export, mirroring
    docs/05-midi-mapping.md. */
class MidiMapCard : public juce::Component, private KitEditController::Listener
{
public:
    MidiMapCard(KitEditController& controllerToUse, ToastOverlay& toastsToUse, ModalHost& modalHostToUse);
    ~MidiMapCard() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    int getPreferredHeight() const;

private:
    void kitEditorStateChanged() override { repaint(); }

    KitEditController& controller;
    ToastOverlay& toasts;
    ModalHost& modalHost;
    juce::Label titleLabel;
    juce::TextButton exportButton { "EXPORT" };
    juce::TextButton importButton { "IMPORT" };
    juce::TextButton editButton { "EDIT MIDI MAP" };
};

/** The whole right-hand GLOBAL SETTINGS aside. */
class SidePanel : public juce::Component
{
public:
    SidePanel(KitEditController& controllerToUse, ToastOverlay& toastsToUse, ModalHost& modalHostToUse);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label eyebrow, tinyStatus;

    juce::Viewport viewport;
    juce::Component content;

    VelocityThresholdsCard velocityCard;
    MasterCard masterCard;
    MeterCard meterCard;
    MidiMapCard midiMapCard;
};

} // namespace dsl::ui
