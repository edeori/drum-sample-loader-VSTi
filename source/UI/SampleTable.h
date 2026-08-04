#pragma once

#include "DrumVaultLookAndFeel.h"
#include "KitEditController.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace dsl::ui
{

/** The mic-channel x round-robin-slot grid for the selected instrument's
    selected velocity layer (docs/03-editor-ui.md, section 3.3). Left-click a
    cell to load/replace a sample; click the small × on a filled cell (or
    right-click anywhere in it) to clear it. Click the × on a slot's column
    header to remove that whole round-robin variation, samples and all.
    Sized to its full content — meant to live inside a juce::Viewport for
    scrolling.
*/
class SampleTable : public juce::Component
{
public:
    explicit SampleTable(KitEditController& controllerToUse);

    void refresh();

    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    // Fired after an "IMPORT SAMPLES" bulk-import completes, so the owner
    // (which has access to a ToastOverlay) can report the outcome.
    std::function<void(KitEditController::SampleImportResult)> onImportComplete;

private:
    struct CellRect
    {
        juce::Rectangle<int> bounds;
        juce::String micChannelId;
        int slotIndex = 0;
    };

    struct SlotHeaderRect
    {
        juce::Rectangle<int> bounds;
        int slotIndex = 0;
    };

    void rebuildLayout();
    void promptRemoveSlot(int slotIndex);
    static juce::Rectangle<int> getClearButtonBounds(juce::Rectangle<int> cellBounds);
    static juce::Rectangle<int> getSlotRemoveButtonBounds(juce::Rectangle<int> headerBounds);

    KitEditController& controller;
    std::vector<CellRect> cellRects;
    std::vector<SlotHeaderRect> slotHeaderRects;
    juce::Rectangle<int> addSlotHeaderBounds;
    juce::Rectangle<int> importHeaderBounds;
    juce::Rectangle<int> stereoImportModeBounds;
    bool forceStereoImport = false;
    int hoveredClearButtonIndex = -1;
    int hoveredSlotRemoveIndex = -1;

    static constexpr int rowHeaderWidth = 130;
    static constexpr int rowHeight = 39;
    static constexpr int headerHeight = 28;
    static constexpr int colWidth = 150;
};

} // namespace dsl::ui
