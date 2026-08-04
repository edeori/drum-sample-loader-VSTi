#include "PluginEditor.h"

namespace dsl::ui
{

DrumVaultEditor::DrumVaultEditor(DrumSampleLoaderAudioProcessor& processorToUse)
    : juce::AudioProcessorEditor(processorToUse),
      controller(processorToUse),
      topBar(controller, toastOverlay, modalHost, [this](ViewMode mode) { currentView = mode; resized(); }),
      kitOverviewPanel(controller, modalHost),
      instrumentEditorPanel(controller, toastOverlay, modalHost),
      mixerPanel(controller),
      kitStageView(controller),
      globalMixerView(controller),
      sidePanel(controller, toastOverlay, modalHost, [this]
                {
                    currentView = ViewMode::KitEditor;
                    resized();
                })
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(topBar);
    addAndMakeVisible(kitOverviewPanel);
    addAndMakeVisible(instrumentEditorPanel);
    addAndMakeVisible(mixerPanel);
    addAndMakeVisible(kitStageView);
    addAndMakeVisible(globalMixerView);
    addAndMakeVisible(sidePanel);

    addAndMakeVisible(toastOverlay);
    addChildComponent(modalHost); // stays hidden until ModalHost::show() is called
    modalHost.toFront(false);

    setResizable(true, true);
    setResizeLimits(1000, 640, 2400, 1500);
    setSize(1400, 860);
}

DrumVaultEditor::~DrumVaultEditor()
{
    setLookAndFeel(nullptr);
}

void DrumVaultEditor::paint(juce::Graphics& g)
{
    g.fillAll(Colours::bg);
}

void DrumVaultEditor::resized()
{
    auto area = getLocalBounds();
    topBar.setBounds(area.removeFromTop(58));

    kitOverviewPanel.setVisible(currentView == ViewMode::KitEditor);
    instrumentEditorPanel.setVisible(currentView == ViewMode::KitEditor);
    mixerPanel.setVisible(currentView == ViewMode::KitEditor);
    kitStageView.setVisible(currentView == ViewMode::KitStage);
    globalMixerView.setVisible(currentView == ViewMode::GlobalMixer);
    sidePanel.setVisible(currentView == ViewMode::GlobalSettings);

    if (currentView == ViewMode::GlobalSettings)
    {
        auto contentArea = area.reduced(14);
        constexpr int maxWidth = 900;
        if (contentArea.getWidth() > maxWidth)
            contentArea = contentArea.withSizeKeepingCentre(maxWidth, contentArea.getHeight());
        sidePanel.setBounds(contentArea);
    }
    else if (currentView == ViewMode::KitStage)
    {
        kitStageView.setBounds(area.reduced(14));
    }
    else if (currentView == ViewMode::GlobalMixer)
    {
        globalMixerView.setBounds(area.reduced(14));
    }
    else
    {
        auto mainArea = area.reduced(14);

        constexpr int mixerHeight = 190; // toolbar (39) + strip content (name/fader/value ≈ 140) + margin
        auto mixerArea = mainArea.removeFromBottom(mixerHeight);
        mainArea.removeFromBottom(12);

        const int overviewHeight = (int) (mainArea.getHeight() * 0.42f);
        kitOverviewPanel.setBounds(mainArea.removeFromTop(overviewHeight));
        mainArea.removeFromTop(12);

        instrumentEditorPanel.setBounds(mainArea);
        mixerPanel.setBounds(mixerArea);
    }

    toastOverlay.setBounds(getLocalBounds());
    modalHost.setBounds(getLocalBounds());
}

} // namespace dsl::ui
