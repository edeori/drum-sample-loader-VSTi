#include "Dialogs.h"
#include "MidiNoteUtils.h"
#include <cmath>

namespace dsl::ui
{
namespace
{
void paintDialogChrome(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    paintPanelBackground(g, bounds, 7.0f);
    g.setColour(Colours::blue.withAlpha(0.42f));
    g.drawHorizontalLine(1, 10.0f, bounds.getRight() - 10.0f);
    paintHardwareScrew(g, { 11.0f, 11.0f }, 4.0f);
    paintHardwareScrew(g, { bounds.getRight() - 11.0f, 11.0f }, 4.0f);
}
}

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

namespace
{
    // typeBox item order - index i's combo item id is i+1.
    constexpr InstrumentTypePreset addInstrumentTypePresets[] = {
        InstrumentTypePreset::Kick, InstrumentTypePreset::Snare, InstrumentTypePreset::Tom,
        InstrumentTypePreset::HiHat, InstrumentTypePreset::Crash, InstrumentTypePreset::Ride,
        InstrumentTypePreset::China, InstrumentTypePreset::Splash, InstrumentTypePreset::Effect,
    };
}

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

    nameEditor.setTextToShowWhenEmpty("e.g. Splash 2", Colours::muted);
    nameEditor.setSelectAllWhenFocused(true);
    addAndMakeVisible(nameEditor);

    typeLabel.setText("TYPE", juce::dontSendNotification);
    typeLabel.setFont(DrumVaultLookAndFeel::displayFont(10.0f));
    typeLabel.setColour(juce::Label::textColourId, Colours::muted);
    addAndMakeVisible(typeLabel);

    int itemId = 1;
    for (auto preset : addInstrumentTypePresets)
        typeBox.addItem(DefaultKitFactory::displayNameForTypePreset(preset), itemId++);
    typeBox.setSelectedId(1, juce::dontSendNotification); // Kick
    addAndMakeVisible(typeBox);

    noteFieldLabel.setText("MIDI NOTE", juce::dontSendNotification);
    noteFieldLabel.setFont(DrumVaultLookAndFeel::displayFont(10.0f));
    noteFieldLabel.setColour(juce::Label::textColourId, Colours::muted);
    addAndMakeVisible(noteFieldLabel);

    for (int n = 0; n <= 127; ++n)
        noteBox.addItem(midiNoteToName(n), n + 1);
    noteBox.setSelectedId(62, juce::dontSendNotification); // note 61 = C#3
    addAndMakeVisible(noteBox);

    addAndMakeVisible(cancelButton);
    addAndMakeVisible(confirmButton);
    confirmButton.getProperties().set("primary", true);

    cancelButton.onClick = [this] { host.dismiss(); };
    confirmButton.onClick = [this]
    {
        const auto name = nameEditor.getText().trim();
        if (name.isEmpty())
            return;

        constexpr int numPresets = (int) (sizeof(addInstrumentTypePresets) / sizeof(addInstrumentTypePresets[0]));
        const int presetIndex = juce::jlimit(0, numPresets - 1, typeBox.getSelectedId() - 1);
        const int note = noteBox.getSelectedId() - 1;
        controller.addInstrumentFromType(name, addInstrumentTypePresets[(size_t) presetIndex], note);
        host.dismiss();
    };
}

void AddInstrumentDialog::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    paintDialogChrome(g, b);

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

    typeLabel.setBounds(area.removeFromTop(14));
    typeBox.setBounds(area.removeFromTop(36));
    area.removeFromTop(14);

    noteFieldLabel.setBounds(area.removeFromTop(14));
    noteBox.setBounds(area.removeFromTop(36));
    area.removeFromTop(14);

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
    paintDialogChrome(g, b);
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

// --------------------------------------------------------- ExportDrumsetNameDialog

ExportDrumsetNameDialog::ExportDrumsetNameDialog(ModalHost& hostToUse, const juce::String& initialName,
                                                 std::function<void(juce::String)> onConfirmedToUse)
    : host(hostToUse), onConfirmed(std::move(onConfirmedToUse))
{
    titleLabel.setText("Export drumset", juce::dontSendNotification);
    titleLabel.setFont(DrumVaultLookAndFeel::displayFont(24.0f));
    addAndMakeVisible(titleLabel);

    nameFieldLabel.setText("SAVE AS", juce::dontSendNotification);
    nameFieldLabel.setFont(DrumVaultLookAndFeel::displayFont(10.0f));
    nameFieldLabel.setColour(juce::Label::textColourId, Colours::muted);
    addAndMakeVisible(nameFieldLabel);

    nameEditor.setText(initialName);
    nameEditor.setSelectAllWhenFocused(true);
    nameEditor.onReturnKey = [this] { confirmButton.triggerClick(); };
    addAndMakeVisible(nameEditor);

    addAndMakeVisible(cancelButton);
    addAndMakeVisible(confirmButton);
    confirmButton.getProperties().set("primary", true);

    cancelButton.onClick = [this] { host.dismiss(); };
    confirmButton.onClick = [this]
    {
        const auto name = nameEditor.getText().trim();
        if (name.isEmpty())
            return;

        auto confirmed = onConfirmed;
        host.dismiss();
        if (confirmed)
            confirmed(name);
    };
}

void ExportDrumsetNameDialog::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    paintDialogChrome(g, b);

    g.setColour(Colours::muted);
    g.setFont(DrumVaultLookAndFeel::displayFont(10.0f));
    g.drawText("PORTABLE DRUMSET", 18, 14, getWidth() - 36, 14, juce::Justification::centredLeft);
}

void ExportDrumsetNameDialog::resized()
{
    auto area = getLocalBounds().reduced(18);
    area.removeFromTop(14); // eyebrow drawn in paint()
    titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    nameFieldLabel.setBounds(area.removeFromTop(14));
    nameEditor.setBounds(area.removeFromTop(36));

    auto actions = area.removeFromBottom(36);
    confirmButton.setBounds(actions.removeFromRight(110));
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
    paintDialogChrome(g, b);

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

// ----------------------------------------------------------------- ManualDialog

namespace
{
    struct ManualSectionText { const char* heading; const char* body; };

    const ManualSectionText manualSections[] = {
        { "Overview",
          "Swarm is a multi-channel drum sample instrument built around velocity layers, "
          "non-repeating round robins, per-channel routing and a let-it-ring playback model. "
          "The usual workflow: build or edit a kit in the Kit Editor, load samples per "
          "instrument and velocity layer, check the MIDI mapping, then route and mix each "
          "channel in the Global Mixer. The four top-bar tabs (KIT, KIT EDITOR, GLOBAL MIXER, "
          "GLOBAL SETTINGS) are always available and switch the whole view instantly." },
        { "Kit Editor: Instruments & Articulations",
          "The Kit Overview grid lists every instrument in the kit; click a card to select it, "
          "or use the tile at the end of the grid (\"+ ADD INSTRUMENT\") to add a new one. The "
          "Add Instrument dialog offers real per-piece presets (Kick, Snare, Tom, Hi-Hat, Crash, "
          "Ride, China, Splash, Effect) so a recreated piece gets its correct mic-channel layout "
          "back - Kick gets In/Out, Snare gets Top/Bottom, everything else gets one Spot "
          "channel. An instrument can have several articulations - independently triggered "
          "playing styles such as a Hi-hat's Closed/Open zones or a Crash's Hit/Choke - each "
          "with its own MIDI note and sample pool, selectable from the dropdown next to the "
          "instrument name. RENAME and the delete button live in the instrument header; "
          "the gear icon opens Channel Settings, which changes whether Overhead/Room is captured "
          "as a discrete L/R pair or one stereo channel." },
        { "Loading Samples & Round-Robins",
          "Each velocity layer (Ghost/Soft/Medium/Heavy by default) has its own sample grid: one "
          "row per mic channel, one column per round-robin slot. Click \"+ ADD SLOT\" to add a "
          "take, then click any \"+ Load sample\" cell to assign a file to that channel for that "
          "slot - every mic channel needs its own click, they don't share files. For a whole "
          "folder of multitracked takes at once, use \"IMPORT SAMPLES...\": it matches filenames "
          "to mic channels automatically (Top/Bottom, OH/Overheads, Room, take numbers, stereo "
          "markers like ST) and groups same-take files into one round-robin slot. IMPORT MODE "
          "toggles between automatic channel-count detection and forcing every ambience file to "
          "be treated as an interleaved stereo pair." },
        { "Velocity Layers",
          "The number of velocity layers and their volume thresholds are global (Global Settings "
          "-> VELOCITY THRESHOLDS), applied to every instrument at once. Dragging a threshold "
          "slider changes where one layer ends and the next begins; RESET restores an even split. "
          "Shrinking the layer count doesn't delete samples - they become \"orphaned\" and can "
          "be reassigned once a matching layer exists again, viewable via the \"VIEW ORPHANED "
          "SAMPLES\" button." },
        { "MIDI Mapping & MIDI Learn",
          "Every articulation has its own trigger note. To change one, either pick a note from "
          "the MIDI NOTE dropdown in the instrument header, or click MIDI LEARN and then play the "
          "note on your controller - the plugin captures the next incoming note-on instead of "
          "playing it. Assigning a note already used elsewhere prompts for confirmation before it "
          "reassigns and clears the previous owner. Global Settings -> MIDI MAP lists every "
          "instrument/articulation/note in one table, and can EXPORT the whole map to a JSON file "
          "or IMPORT one back in - handy for matching a specific electronic kit's factory "
          "layout, or reusing a mapping across kits." },
        { "Global Mixer",
          "The Global Mixer is where output routing to the host's real multi-out buses happens: "
          "one channel strip per instrument's spot mic(s), plus pooled Overheads and Room strips "
          "(those two mics are physically shared across the whole kit, so they're routed and "
          "muted/soloed as one kit-wide choice, not per instrument). Each strip has a fader, pan, "
          "output bus dropdown, Mute/Solo, and a small swap button (two opposing arrows) between "
          "Mute/Solo and the fader that mirrors the channel left-to-right - mainly useful on "
          "panned or stereo-ish "
          "channels like Overheads/Room. The Master strip on the right trims the whole kit's "
          "output on top of every channel's own routing, and its meter shows the plugin's actual "
          "final output level. If the host (or a plain stereo Standalone output) doesn't expose "
          "as many channels as the kit's routing calls for, affected channels automatically fall "
          "back to the internal stereo mix rather than going silent." },
        { "Global Settings",
          "Besides Velocity Thresholds and the MIDI Map (covered above), this tab has the MASTER "
          "card (overall volume, pooled Overhead/Room trim, and global pitch tuning in semitones) "
          "and a MASTER OUTPUT peak meter. Every dB value on these faders - and on every fader "
          "throughout the plugin - can be typed directly: double-click the \"X.X dB\" readout "
          "under any fader to enter an exact number instead of dragging." },
        { "Kit Stage",
          "The KIT tab shows an interactive, clickable drum kit - click a piece to preview it "
          "at that articulation's current velocity layer, useful for quickly auditioning a kit "
          "without hunting through the instrument grid. A hit pad briefly animates when the "
          "matching MIDI note is played, from either the on-screen click or an incoming note." },
        { "Standalone: Audio/MIDI Setup",
          "When running as the Standalone app, the AUDIO/MIDI button in the top bar (next to "
          "SAVE) opens the audio/MIDI device settings: pick the output device and sample rate, "
          "and check or uncheck which MIDI inputs actually drive the plugin - this is where "
          "an electronic drum kit's MIDI output gets connected, alongside or instead of a "
          "computer keyboard/mouse controller. Newly connected MIDI devices are enabled "
          "automatically; disable ones you don't want feeding notes into the kit." },
        { "Drumset Export",
          "The DRUMSET menu in the top bar bundles the current kit's preset and every sample it "
          "references into one self-contained, portable folder - useful for archiving a kit or "
          "handing it to someone else without chasing down original sample paths. \"Update "
          "Drumset Export...\" refreshes an already-exported folder in place (re-copying changed "
          "samples and the preset) while preserving any unrelated files placed inside it." },
    };
}

ManualDialog::ManualContent::ManualContent(int contentWidth)
{
    constexpr int margin = 20;
    constexpr int headingHeight = 22;
    constexpr int gapAfterHeading = 6;
    constexpr int gapBetweenSections = 22;
    const int bodyWidth = contentWidth - margin * 2;

    int y = margin;
    for (auto& text : manualSections)
    {
        Section section;
        section.heading = text.heading;
        section.headingBounds = { margin, y, bodyWidth, headingHeight };
        y += headingHeight + gapAfterHeading;

        juce::AttributedString attr;
        attr.setText(text.body);
        attr.setFont(DrumVaultLookAndFeel::bodyFont(12.5f));
        attr.setColour(Colours::muted);
        attr.setLineSpacing(5.0f);
        section.body.createLayout(attr, (float) bodyWidth);

        const int bodyHeight = (int) std::ceil(section.body.getHeight());
        section.bodyBounds = { margin, y, bodyWidth, bodyHeight };
        y += bodyHeight + gapBetweenSections;

        sections.push_back(std::move(section));
    }

    setSize(contentWidth, y);
}

void ManualDialog::ManualContent::paint(juce::Graphics& g)
{
    for (auto& section : sections)
    {
        g.setColour(Colours::blueBright);
        g.setFont(DrumVaultLookAndFeel::displayFont(13.0f));
        g.drawText(section.heading, section.headingBounds, juce::Justification::centredLeft);

        section.body.draw(g, section.bodyBounds.toFloat());
    }
}

ManualDialog::ManualDialog(ModalHost& hostToUse)
    // Content is laid out once, up front, at a fixed width — the dialog
    // itself never resizes (ModalHost always shows it at width x height).
    // Must stay narrower than resized()'s viewport (width - 18*2 margins)
    // minus the vertical scrollbar it always shows, or wrapped text clips
    // against the right edge.
    : host(hostToUse), content(width - 18 * 2 - 16)
{
    titleLabel.setText("SWARM - User Manual", juce::dontSendNotification);
    titleLabel.setFont(DrumVaultLookAndFeel::displayFont(18.0f));
    addAndMakeVisible(titleLabel);

    closeButton.onClick = [this] { host.dismiss(); };
    addAndMakeVisible(closeButton);

    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
}

void ManualDialog::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    paintDialogChrome(g, b);
    g.setColour(Colours::line);
    g.drawHorizontalLine(52, 18.0f, (float) getWidth() - 18.0f);
}

void ManualDialog::resized()
{
    auto area = getLocalBounds().reduced(18);
    auto titleRow = area.removeFromTop(34);
    closeButton.setBounds(titleRow.removeFromRight(28));
    titleLabel.setBounds(titleRow);
    area.removeFromTop(10);

    viewport.setBounds(area);
}

} // namespace dsl::ui
