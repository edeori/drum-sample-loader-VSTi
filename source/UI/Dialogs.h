#pragma once

#include "DrumVaultLookAndFeel.h"
#include "KitEditController.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace dsl::ui
{

/** Full-editor dimmed backdrop that hosts one modal content component at a
    time, mirroring the mockup's <dialog>/::backdrop pattern in-window rather
    than spawning a separate OS window. */
class ModalHost : public juce::Component
{
public:
    ModalHost();

    void show(std::unique_ptr<juce::Component> content, int width, int height);
    void dismiss();

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    std::unique_ptr<juce::Component> contentComponent;
    juce::Point<int> contentSize;
};

/** "Add to kit" dialog — name, category (Body/Cymbal), MIDI note. */
class AddInstrumentDialog : public juce::Component
{
public:
    AddInstrumentDialog(KitEditController& controllerToUse, ModalHost& hostToUse);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    KitEditController& controller;
    ModalHost& host;

    juce::Label titleLabel;
    juce::Label nameFieldLabel;
    juce::TextEditor nameEditor;
    juce::Label categoryLabel;
    juce::ToggleButton bodyToggle { "BODY" }, cymbalToggle { "CYMBAL" };
    juce::Label noteFieldLabel;
    juce::ComboBox noteBox;
    juce::ToggleButton stereoOverheadToggle { "Overhead as a single stereo channel" };
    juce::ToggleButton stereoRoomToggle { "Room as a single stereo channel" };
    juce::TextButton cancelButton { "CANCEL" }, confirmButton { "ADD INSTRUMENT" };
};

/** Changes the selected instrument's OH and Room channel topology. */
class ChannelSettingsDialog : public juce::Component
{
public:
    ChannelSettingsDialog(KitEditController& controllerToUse, ModalHost& hostToUse);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    KitEditController& controller;
    ModalHost& host;
    juce::Label titleLabel, helpLabel, overheadLabel, roomLabel, errorLabel;
    juce::ComboBox overheadFormatBox, roomFormatBox;
    juce::TextButton cancelButton { "CANCEL" }, applyButton { "APPLY" };
};

/** Simple single-message informational dialog ("GOT IT"). */
class InfoDialog : public juce::Component
{
public:
    InfoDialog(ModalHost& hostToUse, const juce::String& eyebrow, const juce::String& title, const juce::String& body);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ModalHost& host;
    juce::String eyebrowText, titleText, bodyText;
    juce::TextButton gotItButton { "GOT IT" };
};

} // namespace dsl::ui
