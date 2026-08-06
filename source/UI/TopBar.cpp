#include "TopBar.h"

#if JucePlugin_Build_Standalone
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace dsl::ui
{
namespace
{
constexpr int brandWidth = 170;
constexpr int navigationDesignWidth = 428;
}

TopBar::TopBar(KitEditController& controllerToUse, ToastOverlay& toastsToUse, ModalHost& modalHostToUse,
               std::function<void(ViewMode)> onTabChangedToUse)
    : controller(controllerToUse), toasts(toastsToUse), modalHost(modalHostToUse),
      onTabChanged(std::move(onTabChangedToUse))
{
    mothLogo = loadReadableMothLogo(110);
    brandButton.setButtonText({});
    brandButton.getProperties().set("brand", true);
    brandButton.setTooltip("Open the Swarm manual");
    brandButton.onClick = [this]
    {
        modalHost.show(std::make_unique<ManualDialog>(modalHost), ManualDialog::width, ManualDialog::height);
    };
    addAndMakeVisible(brandButton);

    kitTab.setToggleState(true, juce::dontSendNotification);
    kitTab.setRadioGroupId(2001, juce::dontSendNotification);
    stageTab.setRadioGroupId(2001, juce::dontSendNotification);
    mixerTab.setRadioGroupId(2001, juce::dontSendNotification);
    globalTab.setRadioGroupId(2001, juce::dontSendNotification);

    auto selectOnly = [this](juce::TextButton& active)
    {
        for (auto* b : { &kitTab, &stageTab, &mixerTab, &globalTab })
            b->setToggleState(b == &active, juce::dontSendNotification);
    };

    kitTab.onClick = [this, selectOnly]
    {
        selectOnly(kitTab);
        if (onTabChanged)
            onTabChanged(ViewMode::KitEditor);
    };
    stageTab.onClick = [this, selectOnly]
    {
        selectOnly(stageTab);
        if (onTabChanged)
            onTabChanged(ViewMode::KitStage);
    };
    mixerTab.onClick = [this, selectOnly]
    {
        selectOnly(mixerTab);
        if (onTabChanged)
            onTabChanged(ViewMode::GlobalMixer);
    };
    globalTab.onClick = [this, selectOnly]
    {
        selectOnly(globalTab);
        if (onTabChanged)
            onTabChanged(ViewMode::GlobalSettings);
    };
    addAndMakeVisible(kitTab);
    addAndMakeVisible(stageTab);
    addAndMakeVisible(mixerTab);
    addAndMakeVisible(globalTab);

    newButton.onClick = [this]
    {
        juce::Component::SafePointer<TopBar> safeThis(this);
        juce::NativeMessageBox::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Create new kit")
                .withMessage("Replace the current kit with a new empty default kit? Unsaved changes will be lost.")
                .withButton("Create New")
                .withButton("Cancel"),
            [safeThis](int result)
            {
                if (safeThis != nullptr && result == 0)
                {
                    safeThis->controller.createNewKit();
                    safeThis->toasts.show("New kit created", "Default empty kit loaded");
                }
            });
    };
    newButton.setTooltip("Create a new empty kit");
    addAndMakeVisible(newButton);

    openButton.onClick = [this] { controller.loadPresetViaDialog(); toasts.show("Opening kit..."); };
    openButton.setTooltip("Load preset...");
    addAndMakeVisible(openButton);

    saveButton.onClick = [this] { controller.savePresetAs(); toasts.show("Preset saved"); };
    saveButton.setTooltip("Save preset...");
    addAndMakeVisible(saveButton);

    drumsetButton.onClick = [this] { showDrumsetMenu(); };
    drumsetButton.setTooltip("Export or update a portable drumset with all samples");
    addAndMakeVisible(drumsetButton);

#if JucePlugin_Build_Standalone
    // getInstance() is only non-null when this is actually the running
    // Standalone app (VST3/AU builds share the same compiled object code,
    // so this can't be decided at compile time).
    if (juce::StandalonePluginHolder::getInstance() != nullptr)
    {
        audioMidiButton.onClick = []
        {
            if (auto* holder = juce::StandalonePluginHolder::getInstance())
                holder->showAudioSettingsDialog();
        };
        audioMidiButton.setTooltip("Choose the audio device and which MIDI inputs (e.g. an electronic drum kit) play this kit");
        addAndMakeVisible(audioMidiButton);
    }
#endif
}

void TopBar::showDrumsetMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Export Drumset...");
    menu.addItem(2, "Update Drumset Export...");

    juce::Component::SafePointer<TopBar> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&drumsetButton),
        [safeThis](int result)
        {
            if (safeThis == nullptr || result == 0)
                return;

            const bool update = result == 2;

            auto completion = [safeThis, update](DrumsetExporter::Result exportResult) mutable
            {
                if (safeThis != nullptr)
                    safeThis->handleDrumsetExportResult(std::move(exportResult), update);
            };

            if (update)
            {
                safeThis->toasts.show("Updating drumset export...", "Samples are copied in the background");
                safeThis->controller.updateDrumsetExportViaDialog(std::move(completion));
                return;
            }

            const auto initialName = DrumsetExporter::suggestedDirectoryName(safeThis->controller.getWorkingKit().kitName);
            safeThis->modalHost.show(std::make_unique<ExportDrumsetNameDialog>(safeThis->modalHost, initialName,
                [safeThis, completion](juce::String chosenName) mutable
                {
                    if (safeThis == nullptr)
                        return;

                    safeThis->toasts.show("Exporting drumset...", "Samples are copied in the background");
                    safeThis->controller.exportDrumsetViaDialog(chosenName, std::move(completion));
                }), 420, 170);
        });
}

void TopBar::handleDrumsetExportResult(DrumsetExporter::Result result, bool wasUpdate)
{
    if (result.success)
    {
        const auto detail = juce::String(result.sampleReferences) + " sample reference(s) - "
            + result.packageDirectory.getFileName();
        toasts.show(wasUpdate ? "Drumset export updated" : "Drumset exported", detail);
        return;
    }

    auto message = result.errorMessage;
    if (! result.missingSamplePaths.isEmpty())
    {
        message += "\n\nMissing samples:";
        const int shown = juce::jmin(8, result.missingSamplePaths.size());
        for (int i = 0; i < shown; ++i)
            message += "\n• " + result.missingSamplePaths[i];
        if (result.missingSamplePaths.size() > shown)
            message += "\n• …and " + juce::String(result.missingSamplePaths.size() - shown) + " more";
    }

    juce::NativeMessageBox::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle(wasUpdate ? "Drumset update failed" : "Drumset export failed")
            .withMessage(message)
            .withButton("OK"),
        nullptr);
}

void TopBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff15181d), 0, 0,
                                            juce::Colour(0xff07080b), 0, (float) getHeight(), false));
    g.fillAll();
    paintMetalTexture(g, bounds, 0.16f);

    g.setColour(juce::Colours::white.withAlpha(0.045f));
    g.drawHorizontalLine(1, 4.0f, (float) getWidth() - 4.0f);
    g.setColour(Colours::line);
    g.drawHorizontalLine(getHeight() - 1, 0.0f, (float) getWidth());
    g.setColour(Colours::blue.withAlpha(0.32f));
    g.drawHorizontalLine(getHeight() - 2, (float) brandWidth,
                         (float) brandWidth + (float) navigationDesignWidth);

    g.setColour(juce::Colour(0xffa8adb7));
    g.setFont(DrumVaultLookAndFeel::bodyFont(8.5f));
    g.drawText(withLetterSpacing("MOTH PRODUCTION"), 20, 7, 130, 13, juce::Justification::centredLeft);
    g.setColour(Colours::text);
    g.setFont(DrumVaultLookAndFeel::displayFont(16.0f));
    g.drawText("SWARM", 20, 22, 130, 24, juce::Justification::centredLeft);

    if (mothLogo.isValid())
    {
        const float logoHeight = 22.0f;
        const float logoWidth = logoHeight * (592.0f / 380.0f);
        const juce::Rectangle<float> logoBounds(((float) getWidth() - logoWidth) * 0.5f,
                                                 ((float) getHeight() - logoHeight) * 0.5f,
                                                 logoWidth, logoHeight);
        g.setOpacity(0.72f);
        g.drawImage(mothLogo, logoBounds, juce::RectanglePlacement::centred);
        g.setOpacity(1.0f);
    }

    g.setColour(Colours::line.withAlpha(0.72f));
    g.drawVerticalLine(brandWidth, 0.0f, (float) getHeight());
}

void TopBar::resized()
{
    auto area = getLocalBounds();
    brandButton.setBounds(area.removeFromLeft(brandWidth));

#if JucePlugin_Build_Standalone
    if (audioMidiButton.isVisible())
    {
        audioMidiButton.setBounds(area.removeFromRight(96));
        area.removeFromRight(6);
    }
#endif

    auto right = area.removeFromRight(276);
    saveButton.setBounds(right.removeFromRight(56));
    openButton.setBounds(right.removeFromRight(56));
    newButton.setBounds(right.removeFromRight(56));
    right.removeFromRight(6);
    drumsetButton.setBounds(right);

    // Keep the Moth mark at the geometric centre of the whole editor. All
    // four view tabs form one group to its left, matching the shared header
    // hierarchy while leaving the file/actions group on the right.
    constexpr int logoHalfClearance = 52;
    const int centreX = getWidth() / 2;

    const int availableNavigationWidth = juce::jmax(0, centreX - logoHalfClearance - brandWidth);
    auto leftNav = juce::Rectangle<int>(brandWidth, 0,
                                         juce::jmin(navigationDesignWidth, availableNavigationWidth),
                                         getHeight());
    constexpr int totalDesignWidth = 108 + 118 + 138 + 138;
    auto takeWeighted = [&leftNav](int weight, int remainingWeight)
    {
        const int width = remainingWeight > 0
            ? juce::roundToInt((float) leftNav.getWidth() * (float) weight / (float) remainingWeight)
            : 0;
        return leftNav.removeFromLeft(width);
    };

    int remainingWeight = totalDesignWidth;
    stageTab.setBounds(takeWeighted(108, remainingWeight));
    remainingWeight -= 108;
    kitTab.setBounds(takeWeighted(118, remainingWeight));
    remainingWeight -= 118;
    mixerTab.setBounds(takeWeighted(138, remainingWeight));
    remainingWeight -= 138;
    globalTab.setBounds(leftNav);
}

} // namespace dsl::ui
