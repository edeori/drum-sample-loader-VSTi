#include "TopBar.h"

namespace dsl::ui
{

TopBar::TopBar(KitEditController& controllerToUse, ToastOverlay& toastsToUse, ModalHost& modalHostToUse,
               std::function<void(ViewMode)> onTabChangedToUse)
    : controller(controllerToUse), toasts(toastsToUse), modalHost(modalHostToUse),
      onTabChanged(std::move(onTabChangedToUse))
{
    brandButton.onClick = [this]
    {
        modalHost.show(std::make_unique<InfoDialog>(modalHost, "SWARM", "Built for real drum performances",
            "A multi-channel drum sample instrument with velocity layers, non-repeating round robins, "
            "flexible per-channel routing and a let-it-ring playback model."), 460, 230);
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
            safeThis->toasts.show(update ? "Updating drumset export..." : "Exporting drumset...",
                                  "Samples are copied in the background");

            auto completion = [safeThis, update](DrumsetExporter::Result exportResult) mutable
            {
                if (safeThis != nullptr)
                    safeThis->handleDrumsetExportResult(std::move(exportResult), update);
            };

            if (update)
                safeThis->controller.updateDrumsetExportViaDialog(std::move(completion));
            else
                safeThis->controller.exportDrumsetViaDialog(std::move(completion));
        });
}

void TopBar::handleDrumsetExportResult(DrumsetExporter::Result result, bool wasUpdate)
{
    if (result.success)
    {
        const auto detail = juce::String(result.sampleReferences) + " sample reference(s) · "
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
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff151b20), 0, 0, juce::Colour(0xff0c1115), 0, (float) getHeight(), false));
    g.fillAll();
    g.setColour(juce::Colour(0xff303941));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, (float) getWidth());
    g.drawVerticalLine(220, 0.0f, (float) getHeight());
}

void TopBar::resized()
{
    auto area = getLocalBounds();
    brandButton.setBounds(area.removeFromLeft(220));

    stageTab.setBounds(area.removeFromLeft(108));
    kitTab.setBounds(area.removeFromLeft(118));
    mixerTab.setBounds(area.removeFromLeft(138));
    globalTab.setBounds(area.removeFromLeft(138));

    auto right = area.removeFromRight(276);
    saveButton.setBounds(right.removeFromRight(56));
    openButton.setBounds(right.removeFromRight(56));
    newButton.setBounds(right.removeFromRight(56));
    right.removeFromRight(6);
    drumsetButton.setBounds(right);
}

} // namespace dsl::ui
