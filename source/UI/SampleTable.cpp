#include "SampleTable.h"

namespace dsl::ui
{

SampleTable::SampleTable(KitEditController& controllerToUse)
    : controller(controllerToUse)
{
    rebuildLayout();
}

void SampleTable::refresh()
{
    rebuildLayout();
    repaint();
}

void SampleTable::rebuildLayout()
{
    cellRects.clear();
    slotHeaderRects.clear();

    auto* inst = controller.getSelectedInstrument();
    auto* articulation = controller.getSelectedArticulation();
    const int layerIndex = controller.getSelectedLayerIndex();

    if (inst == nullptr || articulation == nullptr || layerIndex < 0
        || layerIndex >= (int) articulation->velocityLayers.size())
    {
        addSlotHeaderBounds = { rowHeaderWidth, 0, 110, headerHeight };
        stereoImportModeBounds = { rowHeaderWidth + 110, 0, 150, headerHeight };
        importHeaderBounds = { stereoImportModeBounds.getRight(), 0, 140, headerHeight };
        setSize(importHeaderBounds.getRight(), headerHeight);
        return;
    }

    auto& layer = articulation->velocityLayers[(size_t) layerIndex];
    const int numSlots = (int) layer.slots.size();

    for (int s = 0; s < numSlots; ++s)
        slotHeaderRects.push_back({ { rowHeaderWidth + s * colWidth, 0, colWidth, headerHeight }, s });

    int y = headerHeight;
    for (auto& mic : inst->micChannels)
    {
        int x = rowHeaderWidth;
        for (int s = 0; s < numSlots; ++s)
        {
            cellRects.push_back({ { x, y, colWidth, rowHeight }, mic.id, s });
            x += colWidth;
        }
        y += rowHeight;
    }

    addSlotHeaderBounds = { rowHeaderWidth + numSlots * colWidth, 0, 110, headerHeight };
    stereoImportModeBounds = { addSlotHeaderBounds.getRight(), 0, 150, headerHeight };
    importHeaderBounds = { stereoImportModeBounds.getRight(), 0, 140, headerHeight };
    setSize(importHeaderBounds.getRight(), headerHeight + (int) inst->micChannels.size() * rowHeight);
}

juce::Rectangle<int> SampleTable::getClearButtonBounds(juce::Rectangle<int> cellBounds)
{
    constexpr int size = 18;
    return cellBounds.removeFromRight(size + 6).withSizeKeepingCentre(size, size);
}

juce::Rectangle<int> SampleTable::getSlotRemoveButtonBounds(juce::Rectangle<int> headerBounds)
{
    constexpr int size = 16;
    return headerBounds.removeFromRight(size + 6).withSizeKeepingCentre(size, size);
}

void SampleTable::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff131a1f));

    auto* inst = controller.getSelectedInstrument();
    auto* articulation = controller.getSelectedArticulation();
    const int layerIndex = controller.getSelectedLayerIndex();
    if (inst == nullptr || articulation == nullptr || layerIndex < 0
        || layerIndex >= (int) articulation->velocityLayers.size())
        return;

    auto& layer = articulation->velocityLayers[(size_t) layerIndex];

    g.setColour(juce::Colour(0xff10161a));
    g.fillRect(0, 0, getWidth(), headerHeight);
    g.setColour(juce::Colour(0xff151d22));
    g.fillRect(0, 0, rowHeaderWidth, getHeight());

    g.setColour(juce::Colour(0xffaeb8bf));
    g.setFont(DrumVaultLookAndFeel::bodyFont(10.0f));
    g.drawText("MIC CHANNELS", 14, 0, rowHeaderWidth - 14, headerHeight, juce::Justification::centredLeft);

    for (auto& sh : slotHeaderRects)
    {
        g.setColour(juce::Colour(0xff2a353d));
        g.drawRect(sh.bounds);

        const auto removeButton = getSlotRemoveButtonBounds(sh.bounds);
        const auto labelArea = sh.bounds.withRight(removeButton.getX() - 2).reduced(8, 0);
        g.setColour(juce::Colour(0xffaeb8bf));
        g.drawText("Slot " + juce::String(sh.slotIndex + 1), labelArea, juce::Justification::centredLeft);

        const bool hoveredRemove = sh.slotIndex == hoveredSlotRemoveIndex;
        if (hoveredRemove)
        {
            g.setColour(juce::Colour(0xff4a2b2d));
            g.fillRoundedRectangle(removeButton.toFloat().expanded(2.0f), 3.0f);
        }
        g.setColour(hoveredRemove ? juce::Colour(0xffe6b0b0) : juce::Colour(0xff5c6870));
        g.drawText(juce::String::charToString((juce::juce_wchar) 0x2715), removeButton, juce::Justification::centred);
    }

    g.setColour(Colours::blueBright);
    g.setFont(DrumVaultLookAndFeel::displayFont(10.0f));
    g.drawText("+ ADD SLOT", addSlotHeaderBounds, juce::Justification::centred);
    g.setColour(forceStereoImport ? Colours::green : Colours::muted);
    g.drawText(forceStereoImport ? "IMPORT MODE: STEREO" : "IMPORT MODE: AUTO",
               stereoImportModeBounds, juce::Justification::centred);
    g.setColour(Colours::blueBright);
    g.drawText("IMPORT SAMPLES...", importHeaderBounds, juce::Justification::centred);

    int y = headerHeight;
    for (auto& mic : inst->micChannels)
    {
        const juce::Rectangle<int> rb(0, y, rowHeaderWidth, rowHeight);
        g.setColour(juce::Colour(0xffc7d0d7));
        g.setFont(DrumVaultLookAndFeel::bodyFont(11.0f));
        g.drawText(mic.displayName.isNotEmpty() ? mic.displayName : mic.id, rb.reduced(14, 0), juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff283239));
        g.drawHorizontalLine(y + rowHeight, 0.0f, (float) getWidth());
        y += rowHeight;
    }

    for (int i = 0; i < (int) cellRects.size(); ++i)
    {
        auto& cell = cellRects[(size_t) i];
        // cellRects can briefly be stale relative to the model — e.g. a slot
        // was just removed but this repaint landed before refresh() rebuilt
        // the layout (the native "remove slot?" dialog closing can trigger
        // one). Skip anything that no longer has a matching slot rather than
        // indexing out of bounds.
        if (cell.slotIndex < 0 || cell.slotIndex >= (int) layer.slots.size())
            continue;

        const auto& slot = layer.slots[(size_t) cell.slotIndex];
        const auto* sampleRef = slot.find(cell.micChannelId);
        const bool filled = sampleRef != nullptr && sampleRef->sourcePath.isNotEmpty();

        g.setColour(juce::Colour(0xff2a353d));
        g.drawRect(cell.bounds);

        if (filled)
        {
            const auto clearButton = getClearButtonBounds(cell.bounds);
            const auto textArea = cell.bounds.withRight(clearButton.getX() - 2);
            const bool loaded = sampleRef->isLoaded();

            g.setColour(loaded ? juce::Colour(0xffa8b5be) : Colours::red);
            g.setFont(DrumVaultLookAndFeel::bodyFont(9.0f));
            const auto fileName = juce::File(sampleRef->sourcePath).getFileName();
            const auto prefix = loaded
                ? juce::String::charToString((juce::juce_wchar) 0x266a) + " "
                : juce::String("MISSING: ");
            g.drawText(prefix + fileName,
                       textArea.reduced(8, 0), juce::Justification::centredLeft);

            const bool hovered = i == hoveredClearButtonIndex;
            if (hovered)
            {
                g.setColour(juce::Colour(0xff3a4a56));
                g.fillRoundedRectangle(clearButton.toFloat().expanded(2.0f), 3.0f);
            }
            g.setColour(hovered ? juce::Colour(0xffdde4e8) : juce::Colour(0xff5c6870));
            g.drawText(juce::String::charToString((juce::juce_wchar) 0x2715), clearButton, juce::Justification::centred);
        }
        else
        {
            g.setColour(juce::Colour(0xff4b5861));
            g.setFont(DrumVaultLookAndFeel::bodyFont(10.0f));
            g.drawText("+  Load sample", cell.bounds, juce::Justification::centred);
        }
    }
}

void SampleTable::mouseUp(const juce::MouseEvent& e)
{
    if (addSlotHeaderBounds.contains(e.getPosition()))
    {
        controller.addRoundRobinSlot();
        return;
    }

    if (importHeaderBounds.contains(e.getPosition()))
    {
        controller.importSamplesToSelectedInstrument(forceStereoImport,
            [this](KitEditController::SampleImportResult result)
        {
            if (onImportComplete)
                onImportComplete(result);
        });
        return;
    }

    if (stereoImportModeBounds.contains(e.getPosition()))
    {
        forceStereoImport = ! forceStereoImport;
        repaint(stereoImportModeBounds);
        return;
    }

    for (auto& sh : slotHeaderRects)
    {
        if (getSlotRemoveButtonBounds(sh.bounds).contains(e.getPosition()))
        {
            promptRemoveSlot(sh.slotIndex);
            return;
        }
    }

    auto* inst = controller.getSelectedInstrument();
    auto* articulation = controller.getSelectedArticulation();
    const int layerIndex = controller.getSelectedLayerIndex();
    const bool haveLayer = inst != nullptr && articulation != nullptr && layerIndex >= 0
                           && layerIndex < (int) articulation->velocityLayers.size();

    for (auto& cell : cellRects)
    {
        if (! cell.bounds.contains(e.getPosition()))
            continue;

        if (! haveLayer)
            return;

        auto& slots = articulation->velocityLayers[(size_t) layerIndex].slots;
        if (cell.slotIndex < 0 || cell.slotIndex >= (int) slots.size())
            return;

        bool filled = false;
        {
            auto& slot = slots[(size_t) cell.slotIndex];
            const auto* sampleRef = slot.find(cell.micChannelId);
            filled = sampleRef != nullptr && sampleRef->sourcePath.isNotEmpty();
        }

        if (e.mods.isPopupMenu() || (filled && getClearButtonBounds(cell.bounds).contains(e.getPosition())))
            controller.clearSampleCell(cell.micChannelId, cell.slotIndex);
        else
            controller.assignSampleToCell(cell.micChannelId, cell.slotIndex);
        return;
    }
}

void SampleTable::mouseMove(const juce::MouseEvent& e)
{
    auto* inst = controller.getSelectedInstrument();
    auto* articulation = controller.getSelectedArticulation();
    const int layerIndex = controller.getSelectedLayerIndex();
    const bool haveLayer = inst != nullptr && articulation != nullptr && layerIndex >= 0
                           && layerIndex < (int) articulation->velocityLayers.size();

    int newHovered = -1;
    if (haveLayer)
    {
        auto& slots = articulation->velocityLayers[(size_t) layerIndex].slots;
        for (int i = 0; i < (int) cellRects.size(); ++i)
        {
            auto& cell = cellRects[(size_t) i];
            if (cell.slotIndex < 0 || cell.slotIndex >= (int) slots.size())
                continue;

            const auto* sampleRef = slots[(size_t) cell.slotIndex].find(cell.micChannelId);
            const bool filled = sampleRef != nullptr && sampleRef->sourcePath.isNotEmpty();

            if (filled && getClearButtonBounds(cell.bounds).contains(e.getPosition()))
            {
                newHovered = i;
                break;
            }
        }
    }

    int newHoveredSlotRemove = -1;
    for (auto& sh : slotHeaderRects)
    {
        if (getSlotRemoveButtonBounds(sh.bounds).contains(e.getPosition()))
        {
            newHoveredSlotRemove = sh.slotIndex;
            break;
        }
    }

    if (newHovered != hoveredClearButtonIndex || newHoveredSlotRemove != hoveredSlotRemoveIndex)
    {
        hoveredClearButtonIndex = newHovered;
        hoveredSlotRemoveIndex = newHoveredSlotRemove;
        const bool overAnyButton = newHovered != -1 || newHoveredSlotRemove != -1;
        setMouseCursor(overAnyButton ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SampleTable::mouseExit(const juce::MouseEvent&)
{
    if (hoveredClearButtonIndex != -1 || hoveredSlotRemoveIndex != -1)
    {
        hoveredClearButtonIndex = -1;
        hoveredSlotRemoveIndex = -1;
        repaint();
    }
}

void SampleTable::promptRemoveSlot(int slotIndex)
{
    juce::NativeMessageBox::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle("Remove slot")
            .withMessage("Remove Slot " + juce::String(slotIndex + 1) + " and all its samples?")
            .withButton("Remove")
            .withButton("Cancel"),
        [this, slotIndex](int result)
        {
            // NativeMessageBox result is the index of the clicked button in
            // the order added via withButton() — 0 for "Remove", 1 for "Cancel".
            if (result == 0)
                controller.removeRoundRobinSlot(slotIndex);
        });
}

} // namespace dsl::ui
