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

/** "Add to kit" dialog — name, type preset (Kick/Snare/Tom/Hi-Hat/Crash/
    Ride/China/Splash/Effect, each with its own default mic-channel layout),
    MIDI note. Overhead/Room format isn't asked here — it's auto-detected
    from sample filenames on import (SampleImportMatcher) and otherwise
    changeable afterward via the instrument editor's gear-icon Channel
    Settings dialog, so asking twice would just be confusing. */
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
    juce::Label typeLabel;
    juce::ComboBox typeBox;
    juce::Label noteFieldLabel;
    juce::ComboBox noteBox;
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

/** Asks for the folder name a portable drumset export should be saved under,
    prefilled with a name derived from the kit. */
class ExportDrumsetNameDialog : public juce::Component
{
public:
    ExportDrumsetNameDialog(ModalHost& hostToUse, const juce::String& initialName,
                            std::function<void(juce::String)> onConfirmed);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ModalHost& host;
    std::function<void(juce::String)> onConfirmed;

    juce::Label titleLabel;
    juce::Label nameFieldLabel;
    juce::TextEditor nameEditor;
    juce::TextButton cancelButton { "CANCEL" }, confirmButton { "EXPORT" };
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

/** Full plugin usage manual, shown from the SWARM brand button — a
    scrollable, sectioned English how-to-use-it document (as opposed to
    InfoDialog's one-paragraph blurb). Content and layout are fixed at
    construction time since the dialog itself is a fixed-size modal. */
class ManualDialog : public juce::Component
{
public:
    explicit ManualDialog(ModalHost& hostToUse);

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int width = 680;
    static constexpr int height = 600;

private:
    class ManualContent : public juce::Component
    {
    public:
        explicit ManualContent(int contentWidth);
        void paint(juce::Graphics&) override;

    private:
        struct Section
        {
            juce::String heading;
            juce::TextLayout body;
            juce::Rectangle<int> headingBounds, bodyBounds;
        };
        std::vector<Section> sections;
    };

    ModalHost& host;
    juce::Label titleLabel;
    juce::TextButton closeButton { juce::String::charToString((juce::juce_wchar) 0x00d7) };
    juce::Viewport viewport;
    ManualContent content;
};

} // namespace dsl::ui
