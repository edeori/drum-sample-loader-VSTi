#pragma once

#include "Dialogs.h"
#include "DrumVaultLookAndFeel.h"
#include "KitEditController.h"
#include "ToastOverlay.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace dsl::ui
{

enum class ViewMode { KitEditor, GlobalSettings, KitStage, GlobalMixer };

/** Top chrome: brand, the four view tabs (Kit / Kit Editor / Global Mixer /
    Global Settings), clear load/save actions and portable drumset export. */
class TopBar : public juce::Component
{
public:
    TopBar(KitEditController& controllerToUse, ToastOverlay& toastsToUse, ModalHost& modalHostToUse,
           std::function<void(ViewMode)> onTabChanged);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void showDrumsetMenu();
    void handleDrumsetExportResult(DrumsetExporter::Result result, bool wasUpdate);

    KitEditController& controller;
    ToastOverlay& toasts;
    ModalHost& modalHost;
    std::function<void(ViewMode)> onTabChanged;
    juce::Image mothLogo;

    juce::TextButton brandButton { "SWARM" };
    juce::TextButton stageTab { "KIT" }, kitTab { "KIT EDITOR" }, mixerTab { "GLOBAL MIXER" }, globalTab { "GLOBAL SETTINGS" };
    juce::TextButton newButton { "NEW" };
    juce::TextButton openButton { "LOAD" };
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton drumsetButton { "DRUMSET" };

#if JucePlugin_Build_Standalone
    // Only meaningful when actually running as the Standalone app (the
    // shared-code object file is compiled once with every enabled format's
    // JucePlugin_Build_* macro set, so this can't be resolved at compile
    // time — hidden at runtime instead, see the constructor).
    juce::TextButton audioMidiButton { "AUDIO/MIDI" };
#endif
};

} // namespace dsl::ui
