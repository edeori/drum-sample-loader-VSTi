#include "InstrumentEditorPanel.h"
#include "MidiNoteUtils.h"

namespace dsl::ui
{

namespace
{
    std::vector<std::pair<int, int>> velocityRanges(const GlobalVelocityConfig& config)
    {
        std::vector<std::pair<int, int>> ranges;
        int start = 0;
        for (auto t : config.thresholds) { ranges.push_back({ start, t - 1 }); start = t; }
        ranges.push_back({ start, 127 });
        return ranges;
    }
}

InstrumentEditorPanel::InstrumentEditorPanel(KitEditController& controllerToUse, ToastOverlay& toastsToUse, ModalHost& modalHostToUse)
    : controller(controllerToUse), toasts(toastsToUse), modalHost(modalHostToUse), sampleTable(controllerToUse)
{
    nameLabel.setFont(DrumVaultLookAndFeel::displayFont(21.0f));
    nameLabel.setMinimumHorizontalScale(0.72f);
    addAndMakeVisible(nameLabel);

    editNameButton.onClick = [this] { promptRename(); };
    editNameButton.setTooltip("Rename this instrument");
    addAndMakeVisible(editNameButton);

    categoryLabel.setColour(juce::Label::textColourId, Colours::muted);
    categoryLabel.setFont(DrumVaultLookAndFeel::bodyFont(10.0f));
    addAndMakeVisible(categoryLabel);

    articulationBox.onChange = [this]
    {
        controller.selectArticulation(articulationBox.getSelectedId() - 1);
    };
    addAndMakeVisible(articulationBox);

    midiNoteBox.addItem("Unassigned", 1);
    for (int n = 0; n <= 127; ++n)
        midiNoteBox.addItem(midiNoteToName(n), n + 2);
    midiNoteBox.onChange = [this]
    {
        auto* inst = controller.getSelectedInstrument();
        auto* articulation = controller.getSelectedArticulation();
        if (inst == nullptr || articulation == nullptr) return;
        const int note = midiNoteBox.getSelectedId() - 2;
        if (note == articulation->midiNote) return;

        auto result = controller.setArticulationMidiNote(inst->id, articulation->id, note, false);
        if (result == Kit::SetNoteResult::Conflict)
        {
            toasts.show("MIDI note already in use", midiNoteToName(note) + " is assigned to another instrument");
            refresh();
        }
        else
        {
            toasts.show("MIDI note updated", note >= 0 ? midiNoteToName(note) : juce::String("Unassigned"));
        }
    };
    addAndMakeVisible(midiNoteBox);

    midiLearnButton.onClick = [this]
    {
        auto* inst = controller.getSelectedInstrument();
        auto* articulation = controller.getSelectedArticulation();
        if (inst == nullptr || articulation == nullptr) return;

        if (controller.isMidiLearnArmedFor(inst->id, articulation->id))
            controller.cancelMidiLearn();
        else
        {
            controller.beginMidiLearn(inst->id, articulation->id);
            toasts.show("MIDI Learn active", "Play a note on your controller");
        }
    };
    addAndMakeVisible(midiLearnButton);

    previewButton.onClick = [this] { controller.previewSelectedInstrument(); };
    addAndMakeVisible(previewButton);

    settingsButton.onClick = [this]
    {
        modalHost.show(std::make_unique<ChannelSettingsDialog>(controller, modalHost), 500, 300);
    };
    addAndMakeVisible(settingsButton);

    deleteButton.getProperties().set("danger", true);
    deleteButton.onClick = [this] { promptDelete(); };
    addAndMakeVisible(deleteButton);

    addAndMakeVisible(layerTabsStrip);

    sampleTable.onImportComplete = [this](KitEditController::SampleImportResult result)
    {
        if (result.filesAssigned == 0 && result.unmatchedFileNames.isEmpty())
            return;

        juce::String body = juce::String(result.filesAssigned) + " file(s) into " + juce::String(result.slotsCreated) + " new slot(s)";
        if (! result.unmatchedFileNames.isEmpty())
            body << " — " << result.unmatchedFileNames.size() << " couldn't be matched to a mic channel";

        toasts.show("Samples imported", body);
    };

    sampleViewport.setViewedComponent(&sampleTable, false);
    sampleViewport.setScrollBarsShown(true, true);
    addAndMakeVisible(sampleViewport);

    controller.addListener(this);
    refresh();
}

InstrumentEditorPanel::~InstrumentEditorPanel()
{
    controller.removeListener(this);
}

void InstrumentEditorPanel::kitEditorStateChanged() { refresh(); }

void InstrumentEditorPanel::refresh()
{
    auto* inst = controller.getSelectedInstrument();
    if (inst == nullptr)
    {
        nameLabel.setText("No instrument selected", juce::dontSendNotification);
        categoryLabel.setText({}, juce::dontSendNotification);
        setEnabled(false);
        return;
    }

    setEnabled(true);
    iconArt = loadInstrumentArt(*inst);
    nameLabel.setText(inst->name, juce::dontSendNotification);
    categoryLabel.setText((inst->category == InstrumentCategory::Body ? juce::String("Body") : juce::String("Cymbal")),
                           juce::dontSendNotification);

    articulationBox.clear(juce::dontSendNotification);
    for (int i = 0; i < (int) inst->articulations.size(); ++i)
        articulationBox.addItem(inst->articulations[(size_t) i].name, i + 1);
    articulationBox.setSelectedId(controller.getSelectedArticulationIndex() + 1, juce::dontSendNotification);
    articulationBox.setVisible(inst->articulations.size() > 1);

    auto* articulation = controller.getSelectedArticulation();
    midiNoteBox.setSelectedId(articulation != nullptr ? juce::jlimit(1, 129, articulation->midiNote + 2) : 1,
                              juce::dontSendNotification);

    const bool learning = articulation != nullptr && controller.isMidiLearnArmedFor(inst->id, articulation->id);
    midiLearnButton.setButtonText(learning ? "PLAY A NOTE..." : "MIDI LEARN");
    midiLearnButton.setToggleState(learning, juce::dontSendNotification);

    sampleTable.refresh();
    // The articulation selector is conditional. Instrument changes do not
    // resize this component, so explicitly relayout after showing/hiding it;
    // otherwise it keeps a zero-sized bound from the previous single-
    // articulation instrument and is technically visible but impossible to
    // see or use.
    resized();
    repaint();
}

void InstrumentEditorPanel::updateLayerTabs()
{
    layerTabButtons.clear();
    layerTabsStrip.removeAllChildren();

    auto& config = controller.getWorkingKit().velocityConfig;
    auto ranges = velocityRanges(config);
    const int selected = controller.getSelectedLayerIndex();

    for (int i = 0; i < (int) ranges.size(); ++i)
    {
        auto label = (i < (int) config.layerNames.size() ? config.layerNames[(size_t) i] : ("Layer " + juce::String(i + 1)))
                     + " (" + juce::String(ranges[(size_t) i].first) + "-" + juce::String(ranges[(size_t) i].second) + ")";

        auto* btn = layerTabButtons.add(new juce::TextButton(label));
        btn->setToggleState(i == selected, juce::dontSendNotification);
        btn->onClick = [this, i] { controller.selectLayer(i); };
        layerTabsStrip.addAndMakeVisible(btn);
    }

    auto bounds = layerTabsStrip.getLocalBounds();
    if (! ranges.empty() && bounds.getWidth() > 0)
    {
        const int tabW = bounds.getWidth() / (int) ranges.size();
        int x = 0;
        for (auto* btn : layerTabButtons)
        {
            btn->setBounds(x, 0, tabW, bounds.getHeight());
            x += tabW;
        }
    }
}

void InstrumentEditorPanel::promptRename()
{
    auto* inst = controller.getSelectedInstrument();
    if (inst == nullptr)
        return;

    auto* aw = new juce::AlertWindow("Rename instrument", {}, juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor("name", inst->name);
    aw->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    const auto instrumentId = inst->id;
    aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw, instrumentId](int result)
    {
        if (result == 1)
            controller.renameInstrument(instrumentId, aw->getTextEditorContents("name"));
        aw->exitModalState(0);
        delete aw;
    }));
}

void InstrumentEditorPanel::promptDelete()
{
    auto* inst = controller.getSelectedInstrument();
    if (inst == nullptr)
        return;

    const auto instrumentId = inst->id;
    const auto name = inst->name;

    juce::NativeMessageBox::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle("Remove instrument")
            .withMessage("Remove " + name + " from the kit?")
            .withButton("Remove")
            .withButton("Cancel"),
        [this, instrumentId](int result)
        {
            // NativeMessageBox result is the index of the clicked button in
            // the order added via withButton() — 0 for "Remove", 1 for "Cancel".
            if (result == 0)
                controller.removeInstrument(instrumentId);
        });
}

void InstrumentEditorPanel::paint(juce::Graphics& g)
{
    paintPanelBackground(g, getLocalBounds().toFloat());

    if (iconArt.isValid())
    {
        juce::Rectangle<int> iconBounds(13, 13, 58, 50);
        g.drawImage(iconArt, iconBounds.toFloat(), juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }

    g.setColour(Colours::line);
    g.drawHorizontalLine(76, 0.0f, (float) getWidth());
    g.drawHorizontalLine(111, 0.0f, (float) getWidth());
}

void InstrumentEditorPanel::resized()
{
    auto area = getLocalBounds();
    auto head = area.removeFromTop(76).reduced(13, 10);

    head.removeFromLeft(71); // icon space, drawn in paint()

    auto actionsArea = head.removeFromRight(196);
    auto actionRow = actionsArea.withSizeKeepingCentre(actionsArea.getWidth(), 32);
    deleteButton.setBounds(actionRow.removeFromRight(32));
    actionRow.removeFromRight(6);
    settingsButton.setBounds(actionRow.removeFromRight(32));
    actionRow.removeFromRight(6);
    previewButton.setBounds(actionRow);
    head.removeFromRight(10);

    auto nameRow = head.removeFromTop(24);
    editNameButton.setBounds(nameRow.removeFromRight(76));
    nameRow.removeFromRight(7);
    nameLabel.setBounds(nameRow);

    auto metaRow = head.removeFromTop(20);
    categoryLabel.setBounds(metaRow.removeFromLeft(62));
    if (articulationBox.isVisible())
    {
        articulationBox.setBounds(metaRow.removeFromLeft(105));
        metaRow.removeFromLeft(6);
    }
    midiNoteBox.setBounds(metaRow.removeFromLeft(80));
    metaRow.removeFromLeft(8);
    midiLearnButton.setBounds(metaRow.removeFromLeft(100));

    auto tabsArea = area.removeFromTop(35);
    layerTabsStrip.setBounds(tabsArea);
    updateLayerTabs();

    sampleViewport.setBounds(area);
}

} // namespace dsl::ui
