#pragma once

#include "Dialogs.h"
#include "DrumVaultLookAndFeel.h"
#include "InstrumentArt.h"
#include "KitEditController.h"
#include "SampleTable.h"
#include "ToastOverlay.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace dsl::ui
{

/** INSTRUMENT EDITOR panel: header (name/category/MIDI note/learn), velocity
    layer tabs, and the sample table for the selected instrument.
*/
class InstrumentEditorPanel : public juce::Component, private KitEditController::Listener
{
public:
    InstrumentEditorPanel(KitEditController& controllerToUse, ToastOverlay& toastsToUse, ModalHost& modalHostToUse);
    ~InstrumentEditorPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void kitEditorStateChanged() override;
    void refresh();
    void updateLayerTabs();
    void promptRename();
    void promptDelete();

    KitEditController& controller;
    ToastOverlay& toasts;
    ModalHost& modalHost;

    juce::Image iconArt;
    juce::Label nameLabel, categoryLabel;
    juce::TextButton editNameButton { "RENAME" };
    juce::ComboBox articulationBox;
    juce::ComboBox midiNoteBox;
    juce::TextButton midiLearnButton { "MIDI LEARN" };
    juce::TextButton previewButton { juce::String::charToString((juce::juce_wchar) 0x25b6) + juce::String(" PREVIEW") };
    juce::TextButton settingsButton { juce::String::charToString((juce::juce_wchar) 0x2699) };
    juce::TextButton deleteButton { juce::String::charToString((juce::juce_wchar) 0x00d7) };

    juce::Component layerTabsStrip;
    juce::OwnedArray<juce::TextButton> layerTabButtons;

    juce::Viewport sampleViewport;
    SampleTable sampleTable;
};

} // namespace dsl::ui
