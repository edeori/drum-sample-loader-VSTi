#include "KitStageView.h"
#include <BinaryData.h>

namespace dsl::ui
{

// ------------------------------------------------------------------ HotspotPad

HotspotPad::HotspotPad(KitEditController& controllerToUse, juce::String instrumentIdToUse,
                       juce::String articulationIdToUse, juce::String hotspotIdToUse)
    : controller(controllerToUse),
      instrumentId(std::move(instrumentIdToUse)),
      articulationId(std::move(articulationIdToUse)),
      hotspotId(hotspotIdToUse.isNotEmpty() ? std::move(hotspotIdToUse) : instrumentId)
{
    if (auto* inst = controller.getWorkingKit().findInstrumentById(instrumentId))
        art = loadInstrumentArt(*inst);
}

void HotspotPad::setDisplayMode(DisplayMode mode)
{
    displayMode = mode;
    repaint();
}

void HotspotPad::paint(juce::Graphics& g)
{
    auto* inst = controller.getWorkingKit().findInstrumentById(instrumentId);
    if (inst == nullptr)
        return;

    auto b = getLocalBounds().toFloat();

    float glow = 0.0f;
    juce::uint32 newestHitMs = 0;
    for (auto& articulation : inst->articulations)
    {
        if (articulationId.isNotEmpty() && articulation.id != articulationId)
            continue;

        const auto hit = controller.getProcessor().getEngine().getLastHitTimeMs(articulation.midiNote);
        if (hit != 0 && (newestHitMs == 0 || (juce::int32) (hit - newestHitMs) > 0))
            newestHitMs = hit;
    }
    if (newestHitMs != 0)
    {
        constexpr juce::uint32 decayMs = 350;
        const juce::uint32 age = juce::Time::getMillisecondCounter() - newestHitMs;
        if (age < decayMs)
            glow = 1.0f - (float) age / (float) decayMs;
    }

    juce::Rectangle<float> markerArea = b;

    // The kick hotspot is deliberately tall so its whole front head remains
    // easy to click. Its visual centre therefore sits slightly below the
    // natural focal point of the drum head; lift only the hit marker while
    // keeping the existing clickable ellipse unchanged.
    if (displayMode == DisplayMode::Hotspot && instrumentId == "kick")
        markerArea.translate(0.0f, -b.getHeight() * 0.08f);

    if (displayMode == DisplayMode::StandaloneTile)
    {
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1d2429), b.getX(), b.getY(),
                                                juce::Colour(0xff12181c), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle(b.reduced(2.0f), 6.0f);

        if (art.isValid())
        {
            auto artBounds = b.reduced(8.0f);
            artBounds.removeFromBottom(16.0f);
            g.drawImage(art, artBounds, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        }

        g.setColour(Colours::text);
        g.setFont(DrumVaultLookAndFeel::bodyFont(10.0f));
        auto nameArea = b;
        g.drawText(inst->name, nameArea.removeFromBottom(16.0f), juce::Justification::centred);
    }

    // Hit feedback: just a small greyish dot at the marker centre, fading out.
    if (glow > 0.0f)
    {
        constexpr float dotSize = 14.0f;
        const auto dot = juce::Rectangle<float>(dotSize, dotSize).withCentre(markerArea.getCentre());
        g.setColour(juce::Colour(0xffb7c0c7).withAlpha(glow));
        g.fillEllipse(dot);
    }

#if DSL_DEBUG_HOTSPOTS
    g.setColour(juce::Colours::red.withAlpha(0.9f));
    g.drawRect(b, 1.0f);
    g.setFont(9.0f);
    g.drawText(instrumentId, b, juce::Justification::centred);
#endif
}

bool HotspotPad::hitTest(int x, int y)
{
    // Tiles are genuinely rectangular — the whole bounds are fair game.
    // A Hotspot's bounds are only an approximate
    // bounding box around a round cymbal/drum head though, and neighbouring
    // hotspots' boxes can overlap slightly; restricting the clickable area to
    // an ellipse inscribed in the bounds means a click in an overlapping
    // corner falls through to whichever neighbour's disc is actually there,
    // instead of always being claimed by whichever pad happens to be on top.
    if (displayMode != DisplayMode::Hotspot)
        return true;

    const auto b = getLocalBounds().toFloat();
    const float rx = b.getWidth() * 0.5f;
    const float ry = b.getHeight() * 0.5f;
    if (rx <= 0.0f || ry <= 0.0f)
        return false;

    const float nx = ((float) x - b.getCentreX()) / rx;
    const float ny = ((float) y - b.getCentreY()) / ry;
    return (nx * nx + ny * ny) <= 1.0f;
}

void HotspotPad::mouseUp(const juce::MouseEvent& e)
{
    controller.selectInstrument(instrumentId);

    if (articulationId.isNotEmpty())
    {
        if (auto* instrument = controller.getWorkingKit().findInstrumentById(instrumentId))
            for (int i = 0; i < (int) instrument->articulations.size(); ++i)
                if (instrument->articulations[(size_t) i].id == articulationId)
                {
                    controller.selectArticulation(i);
                    break;
                }
    }

    // Where you click within the pad picks the velocity: bottom = softest
    // (Ghost), top = hardest (Heavy) — so every layer is reachable and
    // testable straight from the stage, without needing the editor.
    const float heightPx = (float) juce::jmax(1, getHeight());
    const float fracFromBottom = 1.0f - juce::jlimit(0.0f, 1.0f, (float) e.position.y / heightPx);
    const int velocity = juce::jlimit(1, 127, juce::roundToInt(fracFromBottom * 127.0f));

    controller.previewSelectedInstrument(velocity);
}

void HotspotPad::mouseEnter(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

// -------------------------------------------------------------- KitStageView

KitStageView::KitStageView(KitEditController& controllerToUse)
    : controller(controllerToUse)
{
    stageImage = juce::ImageCache::getFromMemory(BinaryData::drumset_png, BinaryData::drumset_pngSize);
    if (stageImage.isValid())
    {
        disabledStageImage = stageImage.createCopy();
        disabledStageImage.desaturate();
    }

    heading.setText("KIT", juce::dontSendNotification);
    heading.setFont(DrumVaultLookAndFeel::displayFont(16.0f));
    addAndMakeVisible(heading);

    hintLabel.setText("Click a drum or cymbal to play it", juce::dontSendNotification);
    hintLabel.setColour(juce::Label::textColourId, Colours::muted);
    hintLabel.setFont(DrumVaultLookAndFeel::bodyFont(11.0f));
    hintLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(hintLabel);

    controller.addListener(this);
    rebuildPads();
    startTimerHz(30);
}

KitStageView::~KitStageView()
{
    controller.removeListener(this);
    stopTimer();
}

const std::map<juce::String, juce::Rectangle<float>>& KitStageView::defaultHotspots()
{
    // Normalized (0..1) regions within drumset.png, hand-mapped to where each
    // piece actually sits in that photo. Instruments outside this set
    // (custom-added ones) fall back to the bottom overflow strip — see
    // layoutPads().
    //
    // Calibrated against drumset-only3.png via tools/hotspot-calibrator.html.
    static const std::map<juce::String, juce::Rectangle<float>> map = {
        { "crash1", { 0.154f, 0.052f, 0.179f, 0.097f } },
        { "hihat",  { 0.089f, 0.250f, 0.158f, 0.084f } },
        { "splash1", { 0.449f, 0.143f, 0.082f, 0.057f } },
        { "tom1",   { 0.306f, 0.204f, 0.127f, 0.088f } },
        { "tom2",   { 0.499f, 0.205f, 0.125f, 0.088f } },
        { "snare",  { 0.243f, 0.368f, 0.153f, 0.072f } },
        { "kick",   { 0.361f, 0.437f, 0.221f, 0.300f } },
        { "tom3",   { 0.588f, 0.358f, 0.161f, 0.077f } },
        { "tom4",   { 0.713f, 0.425f, 0.156f, 0.104f } },
        { "ride",   { 0.762f, 0.344f, 0.212f, 0.113f } },
        { "ride:bell", { 0.841f, 0.355f, 0.059f, 0.043f } },
        { "china",  { 0.782f, 0.161f, 0.192f, 0.133f } },
        { "crash2", { 0.612f, 0.091f, 0.194f, 0.140f } },
    };
    return map;
}

void KitStageView::rebuildPads()
{
    pads.clear();
    removeAllChildren();
    addAndMakeVisible(heading);
    addAndMakeVisible(hintLabel);

    for (auto& inst : controller.getWorkingKit().instruments)
    {
        // Hi-hat deliberately has no fixed default here (unlike Ride below):
        // leaving this empty makes both the hit-glow and the click-to-preview
        // fall through to the instrument's first articulation (see
        // HotspotPad::paint()/mouseUp() and KitEditController::selectInstrument()),
        // the same "front of the list" rule Kit Overview already uses — so
        // whichever articulation a preset's author placed first is what plays,
        // instead of a second, independently maintained default that can (and
        // did) drift out of sync with the articulation set.
        juce::String defaultArticulationId;
        if (inst.id == "ride" && inst.findArticulation("main") != nullptr)
            defaultArticulationId = "main";

        addAndMakeVisible(pads.add(new HotspotPad(
            controller, inst.id, defaultArticulationId, inst.id)));

        if (inst.id == "ride" && inst.findArticulation("bell") != nullptr)
            addAndMakeVisible(pads.add(new HotspotPad(
                controller, inst.id, "bell", "ride:bell")));
    }

    layoutPads();
    repaint();
}

void KitStageView::paint(juce::Graphics& g)
{
    paintPanelBackground(g, getLocalBounds().toFloat());

    if (stageImage.isValid() && ! imageBounds.isEmpty())
    {
        g.drawImage(stageImage, imageBounds.toFloat(), juce::RectanglePlacement::centred);

        // The photo always contains the complete physical kit. Keep those
        // fixed positions visible, but desaturate and dim any instrument that
        // is absent from the current preset so an unused splash/tom/etc. does
        // not look playable. There is deliberately no disabled component on
        // top, so these regions also remain non-interactive.
        for (auto& [hotspotId, normalisedBounds] : defaultHotspots())
        {
            if (hotspotId.containsChar(':')
                || controller.getWorkingKit().findInstrumentById(hotspotId) != nullptr)
                continue;

            const juce::Rectangle<float> bounds(
                (float) imageBounds.getX() + normalisedBounds.getX() * (float) imageBounds.getWidth(),
                (float) imageBounds.getY() + normalisedBounds.getY() * (float) imageBounds.getHeight(),
                normalisedBounds.getWidth() * (float) imageBounds.getWidth(),
                normalisedBounds.getHeight() * (float) imageBounds.getHeight());

            juce::Path mask;
            mask.addEllipse(bounds);
            juce::Graphics::ScopedSaveState clipped(g);
            g.reduceClipRegion(mask);
            if (disabledStageImage.isValid())
                g.drawImage(disabledStageImage, imageBounds.toFloat(), juce::RectanglePlacement::centred);
            g.setColour(juce::Colour(0xff11171b).withAlpha(0.52f));
            g.fillEllipse(bounds);
        }
    }
}

void KitStageView::resized()
{
    auto area = getLocalBounds().reduced(2);
    auto headRow = area.removeFromTop(38).reduced(16, 0);
    heading.setBounds(headRow.removeFromLeft(200));
    hintLabel.setBounds(headRow);

    stageArea = area.reduced(20, 12);
    layoutPads();
}

void KitStageView::layoutPads()
{
    if (stageImage.isValid() && stageArea.getWidth() > 0 && stageArea.getHeight() > 0)
    {
        const double imgAspect = (double) stageImage.getWidth() / (double) stageImage.getHeight();
        const double areaAspect = (double) stageArea.getWidth() / (double) stageArea.getHeight();
        int w, h;
        if (areaAspect > imgAspect) { h = stageArea.getHeight(); w = juce::roundToInt((double) h * imgAspect); }
        else { w = stageArea.getWidth(); h = juce::roundToInt((double) w / imgAspect); }
        imageBounds = juce::Rectangle<int>(0, 0, w, h).withCentre(stageArea.getCentre());
    }
    else
    {
        imageBounds = {};
    }

    auto& hotspotMap = defaultHotspots();
    std::vector<HotspotPad*> unmapped;

    for (auto* pad : pads)
    {
        auto it = hotspotMap.find(pad->getHotspotId());
        if (it != hotspotMap.end() && ! imageBounds.isEmpty())
        {
            auto& nr = it->second;
            const juce::Rectangle<int> r(
                imageBounds.getX() + juce::roundToInt(nr.getX() * (float) imageBounds.getWidth()),
                imageBounds.getY() + juce::roundToInt(nr.getY() * (float) imageBounds.getHeight()),
                juce::roundToInt(nr.getWidth() * (float) imageBounds.getWidth()),
                juce::roundToInt(nr.getHeight() * (float) imageBounds.getHeight()));
            pad->setDisplayMode(HotspotPad::DisplayMode::Hotspot);
            pad->setBounds(r);
        }
        else
        {
            unmapped.push_back(pad);
        }
    }

    // Every custom-added instrument (cymbal or otherwise) has no fixed spot
    // on the photo, so it always lands in the bottom overflow strip.
    if (! unmapped.empty())
    {
        constexpr int overflowHeight = 90;
        const juce::Rectangle<int> overflowArea(stageArea.getX(), stageArea.getBottom() - overflowHeight,
                                                 stageArea.getWidth(), overflowHeight);
        const int padW = overflowArea.getWidth() / (int) unmapped.size();
        int x = overflowArea.getX();
        for (auto* pad : unmapped)
        {
            pad->setDisplayMode(HotspotPad::DisplayMode::StandaloneTile);
            pad->setBounds(x, overflowArea.getY(), padW - 8, overflowArea.getHeight());
            x += padW;
        }
    }
}

} // namespace dsl::ui
