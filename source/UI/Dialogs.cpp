#include "Dialogs.h"
#include "MidiNoteUtils.h"

namespace dsl::ui
{

// ---------------------------------------------------------------- ModalHost

ModalHost::ModalHost()
{
    setVisible(false);
    setInterceptsMouseClicks(true, true);
}

void ModalHost::show(std::unique_ptr<juce::Component> content, int width, int height)
{
    removeAllChildren();
    contentComponent = std::move(content);
    addAndMakeVisible(*contentComponent);
    contentSize = { width, height };
    setVisible(true);
    toFront(true);
    resized();
}

void ModalHost::dismiss()
{
    setVisible(false);
    removeAllChildren();
    contentComponent.reset();
}

void ModalHost::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff000305).withAlpha(0.78f));
}

void ModalHost::resized()
{
    if (contentComponent != nullptr)
        contentComponent->setBounds(getLocalBounds().withSizeKeepingCentre(contentSize.x, contentSize.y));
}

void ModalHost::mouseDown(const juce::MouseEvent& e)
{
    if (contentComponent != nullptr && ! contentComponent->getBounds().contains(e.getPosition()))
        dismiss();
}

// -------------------------------------------------------- AddInstrumentDialog

AddInstrumentDialog::AddInstrumentDialog(KitEditController& controllerToUse, ModalHost& hostToUse)
    : controller(controllerToUse), host(hostToUse)
{
    titleLabel.setText("Add to kit", juce::dontSendNotification);
    titleLabel.setFont(DrumVaultLookAndFeel::displayFont(24.0f));
    addAndMakeVisible(titleLabel);

    nameFieldLabel.setText("INSTRUMENT NAME", juce::dontSendNotification);
    nameFieldLabel.setFont(DrumVaultLookAndFeel::displayFont(10.0f));
    nameFieldLabel.setColour(juce::Label::textColourId, Colours::muted);
    addAndMakeVisible(nameFieldLabel);

    nameEditor.setText("China 1");
    nameEditor.setSelectAllWhenFocused(true);
    addAndMakeVisible(nameEditor);

    categoryLabel.setText("CATEGORY", juce::dontSendNotification);
    categoryLabel.setFont(DrumVaultLookAndFeel::displayFont(10.0f));
    categoryLabel.setColour(juce::Label::textColourId, Colours::muted);
    addAndMakeVisible(categoryLabel);

    bodyToggle.setRadioGroupId(1001, juce::dontSendNotification);
    cymbalToggle.setRadioGroupId(1001, juce::dontSendNotification);
    cymbalToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(bodyToggle);
    addAndMakeVisible(cymbalToggle);

    noteFieldLabel.setText("MIDI NOTE", juce::dontSendNotification);
    noteFieldLabel.setFont(DrumVaultLookAndFeel::displayFont(10.0f));
    noteFieldLabel.setColour(juce::Label::textColourId, Colours::muted);
    addAndMakeVisible(noteFieldLabel);

    for (int n = 0; n <= 127; ++n)
        noteBox.addItem(midiNoteToName(n), n + 1);
    noteBox.setSelectedId(62, juce::dontSendNotification); // note 61 = C#3
    addAndMakeVisible(noteBox);

    addAndMakeVisible(stereoOverheadToggle);
    addAndMakeVisible(stereoRoomToggle);

    addAndMakeVisible(cancelButton);
    addAndMakeVisible(confirmButton);
    confirmButton.getProperties().set("primary", true);

    cancelButton.onClick = [this] { host.dismiss(); };
    confirmButton.onClick = [this]
    {
        const auto name = nameEditor.getText().trim();
        if (name.isEmpty())
            return;

        const auto category = cymbalToggle.getToggleState() ? InstrumentCategory::Cymbal : InstrumentCategory::Body;
        const int note = noteBox.getSelectedId() - 1;
        const auto overheadFormat = stereoOverheadToggle.getToggleState() ? AmbienceFormat::Stereo : AmbienceFormat::MonoPair;
        const auto roomFormat = stereoRoomToggle.getToggleState() ? AmbienceFormat::Stereo : AmbienceFormat::MonoPair;
        controller.addInstrument(name, category, note, overheadFormat, roomFormat);
        host.dismiss();
    };
}

void AddInstrumentDialog::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff182127), b.getX(), b.getY(),
                                            juce::Colour(0xff0e1418), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle(b, 6.0f);
    g.setColour(juce::Colour(0xff3a4750));
    g.drawRoundedRectangle(b.reduced(0.5f), 6.0f, 1.0f);

    g.setColour(Colours::muted);
    g.setFont(DrumVaultLookAndFeel::displayFont(10.0f));
    g.drawText("NEW INSTRUMENT", 18, 14, getWidth() - 36, 14, juce::Justification::centredLeft);
}

void AddInstrumentDialog::resized()
{
    auto area = getLocalBounds().reduced(18);
    area.removeFromTop(14); // eyebrow drawn in paint()
    titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    nameFieldLabel.setBounds(area.removeFromTop(14));
    nameEditor.setBounds(area.removeFromTop(36));
    area.removeFromTop(14);

    categoryLabel.setBounds(area.removeFromTop(14));
    auto choiceRow = area.removeFromTop(60);
    bodyToggle.setBounds(choiceRow.removeFromLeft(choiceRow.getWidth() / 2).reduced(0, 0));
    cymbalToggle.setBounds(choiceRow.reduced(0, 0));
    area.removeFromTop(14);

    noteFieldLabel.setBounds(area.removeFromTop(14));
    noteBox.setBounds(area.removeFromTop(36));
    area.removeFromTop(14);

    stereoOverheadToggle.setBounds(area.removeFromTop(24));
    stereoRoomToggle.setBounds(area.removeFromTop(24));

    auto actions = area.removeFromBottom(36);
    confirmButton.setBounds(actions.removeFromRight(150));
    actions.removeFromRight(8);
    cancelButton.setBounds(actions.removeFromRight(90));
}

// ------------------------------------------------------ ChannelSettingsDialog

ChannelSettingsDialog::ChannelSettingsDialog(KitEditController& controllerToUse, ModalHost& hostToUse)
    : controller(controllerToUse), host(hostToUse)
{
    titleLabel.setText("Channel settings", juce::dontSendNotification);
    titleLabel.setFont(DrumVaultLookAndFeel::displayFont(23.0f));
    addAndMakeVisible(titleLabel);

    helpLabel.setText("Choose the sample-channel layout independently for Overhead and Room.",
                      juce::dontSendNotification);
    helpLabel.setColour(juce::Label::textColourId, Colours::muted);
    helpLabel.setFont(DrumVaultLookAndFeel::bodyFont(10.0f));
    helpLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(helpLabel);

    overheadLabel.setText("OVERHEAD / OH / OVERHEADS", juce::dontSendNotification);
    roomLabel.setText("ROOM", juce::dontSendNotification);
    for (auto* label : { &overheadLabel, &roomLabel })
    {
        label->setColour(juce::Label::textColourId, Colours::muted);
        label->setFont(DrumVaultLookAndFeel::displayFont(9.0f));
        addAndMakeVisible(*label);
    }

    for (auto* box : { &overheadFormatBox, &roomFormatBox })
    {
        box->addItem("Separate L / R mono channels", 1);
        box->addItem("Single stereo channel", 2);
        box->setSelectedId(1, juce::dontSendNotification);
        addAndMakeVisible(*box);
    }

    if (auto* instrument = controller.getSelectedInstrument())
        for (auto& channel : instrument->micChannels)
        {
            if (channel.role == MicRole::Overhead && channel.numChannels == 2)
                overheadFormatBox.setSelectedId(2, juce::dontSendNotification);
            if (channel.role == MicRole::Room && channel.numChannels == 2)
                roomFormatBox.setSelectedId(2, juce::dontSendNotification);
        }

    errorLabel.setColour(juce::Label::textColourId, Colours::red);
    errorLabel.setFont(DrumVaultLookAndFeel::bodyFont(9.0f));
    errorLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(errorLabel);

    cancelButton.onClick = [this] { host.dismiss(); };
    applyButton.getProperties().set("primary", true);
    applyButton.onClick = [this]
    {
        const auto overhead = overheadFormatBox.getSelectedId() == 2 ? AmbienceFormat::Stereo : AmbienceFormat::MonoPair;
        const auto room = roomFormatBox.getSelectedId() == 2 ? AmbienceFormat::Stereo : AmbienceFormat::MonoPair;
        if (controller.setSelectedAmbienceFormats(overhead, room))
            host.dismiss();
        else
            errorLabel.setText("The channel format could not be changed.",
                               juce::dontSendNotification);
    };
    addAndMakeVisible(cancelButton);
    addAndMakeVisible(applyButton);
}

void ChannelSettingsDialog::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff182127), b.getX(), b.getY(),
                                            juce::Colour(0xff0e1418), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle(b, 6.0f);
    g.setColour(juce::Colour(0xff3a4750));
    g.drawRoundedRectangle(b.reduced(0.5f), 6.0f, 1.0f);
}

void ChannelSettingsDialog::resized()
{
    auto area = getLocalBounds().reduced(18);
    titleLabel.setBounds(area.removeFromTop(32));
    area.removeFromTop(8);
    helpLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(7);
    overheadLabel.setBounds(area.removeFromTop(14));
    overheadFormatBox.setBounds(area.removeFromTop(30));
    area.removeFromTop(7);
    roomLabel.setBounds(area.removeFromTop(14));
    roomFormatBox.setBounds(area.removeFromTop(30));
    errorLabel.setBounds(area.removeFromTop(24));

    auto actions = area.removeFromBottom(36);
    applyButton.setBounds(actions.removeFromRight(110));
    actions.removeFromRight(8);
    cancelButton.setBounds(actions.removeFromRight(90));
}

// ------------------------------------------------------------------ InfoDialog

InfoDialog::InfoDialog(ModalHost& hostToUse, const juce::String& eyebrow, const juce::String& title, const juce::String& body)
    : host(hostToUse), eyebrowText(eyebrow), titleText(title), bodyText(body)
{
    addAndMakeVisible(gotItButton);
    gotItButton.getProperties().set("primary", true);
    gotItButton.onClick = [this] { host.dismiss(); };
}

void InfoDialog::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff182127), b.getX(), b.getY(),
                                            juce::Colour(0xff0e1418), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle(b, 6.0f);
    g.setColour(juce::Colour(0xff3a4750));
    g.drawRoundedRectangle(b.reduced(0.5f), 6.0f, 1.0f);

    auto area = getLocalBounds().reduced(18);
    g.setColour(Colours::muted);
    g.setFont(DrumVaultLookAndFeel::displayFont(10.0f));
    g.drawText(eyebrowText, area.removeFromTop(28), juce::Justification::centredLeft);

    g.setColour(Colours::text);
    g.setFont(DrumVaultLookAndFeel::displayFont(22.0f));
    g.drawText(titleText, area.removeFromTop(32), juce::Justification::centredLeft);

    area.removeFromTop(8);
    g.setColour(Colours::muted);
    g.setFont(DrumVaultLookAndFeel::bodyFont(12.5f));
    g.drawFittedText(bodyText, area.removeFromTop(area.getHeight() - 50), juce::Justification::topLeft, 6, 1.3f);
}

void InfoDialog::resized()
{
    auto actions = getLocalBounds().reduced(18).removeFromBottom(36);
    gotItButton.setBounds(actions.removeFromRight(110));
}

} // namespace dsl::ui
