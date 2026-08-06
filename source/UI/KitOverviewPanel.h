#pragma once

#include "Dialogs.h"
#include "DrumVaultLookAndFeel.h"
#include "InstrumentArt.h"
#include "KitEditController.h"
#include "MidiNoteUtils.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace dsl::ui
{

/** One tile in the kit overview grid: name, MIDI note, drum art, mute/solo. */
class InstrumentCard : public juce::Component
{
public:
    InstrumentCard(KitEditController& controllerToUse, juce::String instrumentIdToUse);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    KitEditController& controller;
    juce::String instrumentId;
    juce::Image art;
    juce::TextButton muteButton { "M" }, soloButton { "S" };
};

/** KIT OVERVIEW panel: toolbar (search / add / import) + the instrument grid. */
class KitOverviewPanel : public juce::Component, private KitEditController::Listener
{
public:
    KitOverviewPanel(KitEditController& controllerToUse, ModalHost& modalHostToUse);
    ~KitOverviewPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void kitEditorStateChanged() override;
    void refresh();
    void rebuildCards();
    void layoutGrid();
    void showAddInstrumentDialog();

    KitEditController& controller;
    ModalHost& modalHost;

    juce::Label heading, countLabel;
    juce::TextEditor searchBox;

    juce::Viewport viewport;
    juce::Component gridContent;
    juce::OwnedArray<InstrumentCard> cards;
    juce::TextButton addTileButton { "+  ADD INSTRUMENT" };
};

} // namespace dsl::ui
