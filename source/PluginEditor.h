#pragma once

#include "PluginProcessor.h"
#include "UI/Dialogs.h"
#include "UI/DrumVaultLookAndFeel.h"
#include "UI/InstrumentEditorPanel.h"
#include "UI/KitEditController.h"
#include "UI/KitOverviewPanel.h"
#include "UI/KitStageView.h"
#include "UI/MixerPanel.h"
#include "UI/SidePanel.h"
#include "UI/ToastOverlay.h"
#include "UI/TopBar.h"

namespace dsl::ui
{

/** The full "DrumVault" editor shell, built from the interactive mockup in
    mockup/: a topbar switches between four full views — Kit Editor (kit
    overview grid, instrument editor, channel mixer), Kit Stage (interactive,
    clickable/hit-animated kit), Global Mixer (output routing to the host's
    multi-out buses), and Global Settings (velocity thresholds, master, meter,
    MIDI map) — all backed live by KitEditController.
*/
class DrumVaultEditor : public juce::AudioProcessorEditor
{
public:
    explicit DrumVaultEditor(DrumSampleLoaderAudioProcessor& processorToUse);
    ~DrumVaultEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    DrumVaultLookAndFeel lookAndFeel;
    KitEditController controller;

    // Renders hover tooltips for every child that has setTooltip() text set
    // (icon-only buttons in the TopBar, mainly) — one per top-level window.
    juce::TooltipWindow tooltipWindow { this };

    ToastOverlay toastOverlay;
    ModalHost modalHost;

    TopBar topBar;
    KitOverviewPanel kitOverviewPanel;
    InstrumentEditorPanel instrumentEditorPanel;
    MixerPanel mixerPanel;
    KitStageView kitStageView;
    GlobalMixerView globalMixerView;
    SidePanel sidePanel;

    ViewMode currentView = ViewMode::KitEditor;
};

} // namespace dsl::ui
