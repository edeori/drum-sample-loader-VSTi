#include "KitOverviewPanel.h"

namespace dsl::ui
{

// ------------------------------------------------------------- InstrumentCard

InstrumentCard::InstrumentCard(KitEditController& controllerToUse, juce::String instrumentIdToUse)
    : controller(controllerToUse), instrumentId(std::move(instrumentIdToUse))
{
    if (auto* inst = controller.getWorkingKit().findInstrumentById(instrumentId))
    {
        art = loadInstrumentArt(*inst);
        muteButton.setToggleState(inst->muted, juce::dontSendNotification);
        soloButton.setToggleState(inst->solo, juce::dontSendNotification);
    }

    addAndMakeVisible(muteButton);
    addAndMakeVisible(soloButton);

    muteButton.onClick = [this] { controller.toggleMute(instrumentId); };
    soloButton.onClick = [this] { controller.toggleSolo(instrumentId); };
}

void InstrumentCard::paint(juce::Graphics& g)
{
    auto* inst = controller.getWorkingKit().findInstrumentById(instrumentId);
    if (inst == nullptr)
        return;

    const bool selected = instrumentId == controller.getSelectedInstrumentId();
    auto b = getLocalBounds().toFloat();

    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1d2429), b.getX(), b.getY(),
                                            juce::Colour(0xff12181c), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle(b, 4.0f);
    g.setColour(selected ? Colours::blue : juce::Colour(0xff303a42));
    g.drawRoundedRectangle(b.reduced(0.5f), 4.0f, selected ? 1.5f : 1.0f);

    if (selected)
    {
        g.setColour(Colours::blue);
        g.fillEllipse(b.getRight() - 15.0f, b.getY() + 7.0f, 6.0f, 6.0f);
    }

    if (art.isValid())
    {
        auto artBounds = getLocalBounds().removeFromRight(100).removeFromBottom(80).reduced(2);
        g.drawImage(art, artBounds.toFloat(), juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }

    auto textArea = getLocalBounds().reduced(10, 8);
    g.setColour(Colours::text);
    g.setFont(DrumVaultLookAndFeel::displayFont(16.0f));
    g.drawText(inst->name, textArea.removeFromTop(20), juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffb9c2c9));
    g.setFont(DrumVaultLookAndFeel::bodyFont(10.0f));
    const auto* primary = inst->primaryArticulation();
    const auto noteText = inst->articulations.size() > 1
        ? juce::String(inst->articulations.size()) + " articulations"
        : (primary != nullptr && primary->midiNote >= 0 ? midiNoteToName(primary->midiNote) : juce::String("-"));
    g.drawText(noteText, textArea.removeFromTop(14), juce::Justification::centredLeft);
}

void InstrumentCard::resized()
{
    auto controls = getLocalBounds().removeFromBottom(31).removeFromLeft(66).reduced(8, 5);
    muteButton.setBounds(controls.removeFromLeft(27));
    controls.removeFromLeft(4);
    soloButton.setBounds(controls.removeFromLeft(27));
}

void InstrumentCard::mouseUp(const juce::MouseEvent&)
{
    controller.selectInstrument(instrumentId);
}

// ---------------------------------------------------------- KitOverviewPanel

KitOverviewPanel::KitOverviewPanel(KitEditController& controllerToUse, ModalHost& modalHostToUse)
    : controller(controllerToUse), modalHost(modalHostToUse)
{
    heading.setText("KIT OVERVIEW", juce::dontSendNotification);
    heading.setFont(DrumVaultLookAndFeel::displayFont(14.0f));
    addAndMakeVisible(heading);

    countLabel.setColour(juce::Label::textColourId, Colours::muted);
    countLabel.setFont(DrumVaultLookAndFeel::bodyFont(9.0f));
    addAndMakeVisible(countLabel);

    searchBox.setTextToShowWhenEmpty("Search instruments...", Colours::muted);
    searchBox.onTextChange = [this] { rebuildCards(); };
    addAndMakeVisible(searchBox);

    addButton.getProperties().set("primary", true);
    addButton.onClick = [this] { showAddInstrumentDialog(); };
    addAndMakeVisible(addButton);

    addTileButton.onClick = [this] { showAddInstrumentDialog(); };
    viewport.setViewedComponent(&gridContent, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    controller.addListener(this);
    refresh();
}

KitOverviewPanel::~KitOverviewPanel()
{
    controller.removeListener(this);
}

void KitOverviewPanel::kitEditorStateChanged() { refresh(); }

void KitOverviewPanel::refresh()
{
    countLabel.setText(juce::String(controller.getWorkingKit().instruments.size()) + " instruments", juce::dontSendNotification);
    rebuildCards();
}

void KitOverviewPanel::rebuildCards()
{
    cards.clear();
    gridContent.removeAllChildren();

    const auto query = searchBox.getText().trim().toLowerCase();

    for (auto& inst : controller.getWorkingKit().instruments)
    {
        if (query.isNotEmpty())
        {
            juce::String haystack = inst.name;
            for (auto& articulation : inst.articulations)
                haystack << " " << articulation.name << " "
                         << (articulation.midiNote >= 0 ? midiNoteToName(articulation.midiNote) : juce::String());
            haystack = haystack.toLowerCase();
            if (! haystack.contains(query))
                continue;
        }

        auto* card = cards.add(new InstrumentCard(controller, inst.id));
        gridContent.addAndMakeVisible(card);
    }

    gridContent.addAndMakeVisible(addTileButton);
    layoutGrid();
}

void KitOverviewPanel::layoutGrid()
{
    const int cardW = 150, cardH = 108, gap = 8;
    const int available = juce::jmax(cardW, viewport.getWidth() - viewport.getScrollBarThickness());
    const int cols = juce::jmax(1, (available + gap) / (cardW + gap));

    int col = 0, x = 0, y = 0;
    auto placeNext = [&](juce::Component& c)
    {
        c.setBounds(x, y, cardW, cardH);
        if (++col >= cols) { col = 0; x = 0; y += cardH + gap; }
        else x += cardW + gap;
    };

    for (auto* card : cards)
        placeNext(*card);
    placeNext(addTileButton);

    const int totalItems = cards.size() + 1;
    const int totalRows = (totalItems + cols - 1) / cols;
    gridContent.setSize(cols * (cardW + gap) - gap, juce::jmax(cardH, totalRows * (cardH + gap) - gap));
}

void KitOverviewPanel::showAddInstrumentDialog()
{
    modalHost.show(std::make_unique<AddInstrumentDialog>(controller, modalHost), 460, 464);
}

void KitOverviewPanel::paint(juce::Graphics& g)
{
    paintPanelBackground(g, getLocalBounds().toFloat());
}

void KitOverviewPanel::resized()
{
    auto area = getLocalBounds().reduced(1);
    auto toolbar = area.removeFromTop(54).reduced(12, 10);

    auto headingArea = toolbar.removeFromLeft(160);
    heading.setBounds(headingArea.removeFromTop(18));
    countLabel.setBounds(headingArea);

    toolbar.removeFromLeft(15);
    auto searchArea = toolbar.removeFromLeft(juce::jmin(300, toolbar.getWidth() / 3));
    searchBox.setBounds(searchArea.withSizeKeepingCentre(searchArea.getWidth(), 30));

    toolbar.removeFromLeft(15);
    auto addArea = toolbar.removeFromRight(160);
    addButton.setBounds(addArea.withSizeKeepingCentre(addArea.getWidth(), 32));

    viewport.setBounds(area.reduced(12, 0).withTrimmedBottom(12));
    layoutGrid();
}

} // namespace dsl::ui
