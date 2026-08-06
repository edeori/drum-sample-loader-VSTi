#include "SidePanel.h"
#include "MidiNoteUtils.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <optional>

namespace dsl::ui
{

namespace
{
    juce::String panDisplayText(float pan)
    {
        const int amount = juce::roundToInt(std::abs(juce::jlimit(-1.0f, 1.0f, pan)) * 100.0f);
        if (amount == 0)
            return "PAN C";
        return pan < 0.0f ? "PAN L" + juce::String(amount) : "PAN R" + juce::String(amount);
    }
}

// ------------------------------------------------------------- GlobalMixerCard

GlobalMixerCard::GlobalMixerCard(KitEditController& controllerToUse)
    : controller(controllerToUse)
{
    controller.addListener(this);
    rebuildRows();
    startTimerHz(30);
}

GlobalMixerCard::~GlobalMixerCard()
{
    controller.removeListener(this);
}

void GlobalMixerCard::rebuildRows()
{
    rows.clear();

    auto& kit = controller.getWorkingKit();
    auto buses = controller.getAvailableBuses();

    auto addRow = [&](const juce::String& label, float faderValue, const juce::String& currentRoutingBusId,
                       bool muted, bool soloed, std::optional<float> panValue,
                       std::function<void(float)> onFader, std::function<void(juce::String)> onRoute,
                       std::function<void()> onToggleMute, std::function<void()> onToggleSolo,
                       std::function<void(float)> onPan, uint64_t meterKey,
                       std::optional<bool> swapState = std::nullopt,
                       std::function<void()> onToggleSwap = nullptr)
    {
        auto* row = rows.add(new Row());
        row->currentBusId = currentRoutingBusId;
        row->meterKey = meterKey;

        row->nameLabel.setText(label, juce::dontSendNotification);
        row->nameLabel.setFont(DrumVaultLookAndFeel::bodyFont(10.0f));
        row->nameLabel.setJustificationType(juce::Justification::centred);
        row->nameLabel.setMinimumHorizontalScale(0.7f);
        addAndMakeVisible(row->nameLabel);

        row->fader.setSliderStyle(juce::Slider::LinearVertical);
        row->fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        row->fader.setRange(-60.0, 6.0, 0.1);
        row->fader.setValue(faderValue, juce::dontSendNotification);
        row->fader.setScrollWheelEnabled(false);
        addAndMakeVisible(row->fader);

        row->valueLabel.setJustificationType(juce::Justification::centred);
        row->valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc8d0d6));
        row->valueLabel.setFont(DrumVaultLookAndFeel::numericFont(10.0f));
        row->valueLabel.setText(juce::String(faderValue, 1) + " dB", juce::dontSendNotification);
        makeDbLabelEditable(row->valueLabel, row->fader);
        addAndMakeVisible(row->valueLabel);

        auto* rowPtr = row;
        row->fader.onValueChange = [rowPtr, onFader]
        {
            const float v = (float) rowPtr->fader.getValue();
            rowPtr->valueLabel.setText(juce::String(v, 1) + " dB", juce::dontSendNotification);
            onFader(v);
        };

        row->hasPanControl = panValue.has_value();
        if (row->hasPanControl)
        {
            row->pan.setSliderStyle(juce::Slider::LinearHorizontal);
            row->pan.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            row->pan.setRange(-1.0, 1.0, 0.01);
            row->pan.setValue((double) *panValue, juce::dontSendNotification);
            row->pan.setDoubleClickReturnValue(true, 0.0);
            row->pan.setScrollWheelEnabled(false);
            row->pan.onValueChange = [rowPtr, onPan]
            {
                const float value = (float) rowPtr->pan.getValue();
                rowPtr->panValueLabel.setText(panDisplayText(value), juce::dontSendNotification);
                onPan(value);
            };
            addAndMakeVisible(row->pan);

            row->panValueLabel.setText(panDisplayText(*panValue), juce::dontSendNotification);
            row->panValueLabel.setJustificationType(juce::Justification::centred);
            row->panValueLabel.setColour(juce::Label::textColourId, Colours::muted);
            row->panValueLabel.setFont(DrumVaultLookAndFeel::numericFont(9.0f));
            addAndMakeVisible(row->panValueLabel);
        }

        int idToSelect = 0, itemId = 1;
        for (auto& bus : buses)
        {
            row->routeBox.addItem(bus.name, itemId);
            if (bus.id == currentRoutingBusId)
                idToSelect = itemId;
            ++itemId;
        }
        if (idToSelect != 0)
            row->routeBox.setSelectedId(idToSelect, juce::dontSendNotification);

        row->routeBox.onChange = [rowPtr, onRoute, buses]
        {
            const int index = rowPtr->routeBox.getSelectedId() - 1;
            if (index >= 0 && index < (int) buses.size())
            {
                rowPtr->currentBusId = buses[(size_t) index].id;
                onRoute(rowPtr->currentBusId);
            }
        };
        addAndMakeVisible(row->routeBox);

        row->muteButton.setClickingTogglesState(true);
        row->muteButton.setToggleState(muted, juce::dontSendNotification);
        row->muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffbd4549));
        row->muteButton.onClick = std::move(onToggleMute);
        addAndMakeVisible(row->muteButton);

        row->soloButton.setClickingTogglesState(true);
        row->soloButton.setToggleState(soloed, juce::dontSendNotification);
        row->soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd0aa3e));
        row->soloButton.onClick = std::move(onToggleSolo);
        addAndMakeVisible(row->soloButton);

        row->hasSwapControl = swapState.has_value();
        if (row->hasSwapControl)
        {
            // A persistent toggle everywhere: kit-wide for the pooled
            // Overheads/Room rows (Kit::overheadSwapLR/roomSwapLR), per
            // mic-channel for per-instrument Spot rows (Instrument::
            // channelSwapLR). DrumEngine::triggerNoteOn decides at hit time
            // whether that means mirroring pan (mono content) or swapping
            // the two channels outright (genuinely stereo content).
            row->swapButton.setTooltip("Swap L/R");
            row->swapButton.setClickingTogglesState(true);
            row->swapButton.setToggleState(*swapState, juce::dontSendNotification);
            row->swapButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff4f82ba));
            row->swapButton.onClick = std::move(onToggleSwap);
            addAndMakeVisible(row->swapButton);
        }
    };

    for (auto& inst : kit.instruments)
    {
        for (auto& mic : inst.micChannels)
        {
            if (mic.role != MicRole::Spot)
                continue;

            const juce::String instrumentId = inst.id;
            const juce::String channelId = mic.id;
            const juce::String label = inst.name + " " + (mic.displayName.isNotEmpty() ? mic.displayName : mic.id);

            addRow(label, inst.getChannelFaderDb(channelId), inst.getChannelRouting(channelId),
                   inst.isChannelMuted(channelId), inst.isChannelSoloed(channelId),
                   mic.numChannels == 1 ? std::optional<float>(mic.pan) : std::nullopt,
                   [this, instrumentId, channelId](float v) { controller.setChannelFaderDbFor(instrumentId, channelId, v); },
                   [this, instrumentId, channelId](juce::String busId) { controller.setChannelRoutingFor(instrumentId, channelId, busId); },
                   [this, instrumentId, channelId] { controller.toggleChannelMuteFor(instrumentId, channelId); },
                   [this, instrumentId, channelId] { controller.toggleChannelSoloFor(instrumentId, channelId); },
                   [this, instrumentId, channelId](float pan) { controller.setChannelPanFor(instrumentId, channelId, pan); },
                   DrumEngine::meterKeyFor(instrumentId, mic),
                   inst.isChannelSwapped(channelId),
                   [this, instrumentId, channelId] { controller.toggleChannelSwapFor(instrumentId, channelId); });
        }
    }

    addRow("Overheads", kit.masterOverheadDb, kit.overheadBusId, kit.overheadMuted, kit.overheadSolo, std::nullopt,
           [this](float v) { controller.setMasterOverheadDb(v); },
           [this](juce::String busId) { controller.setOverheadBusRouting(busId); },
           [this] { controller.toggleOverheadMute(); },
           [this] { controller.toggleOverheadSolo(); },
           [](float) {},
           DrumEngine::meterKeyFor({}, MicChannelDef { {}, {}, MicRole::Overhead, 1, 0.0f }),
           kit.overheadSwapLR,
           [this] { controller.toggleOverheadSwap(); });

    addRow("Room", kit.masterRoomDb, kit.roomBusId, kit.roomMuted, kit.roomSolo, std::nullopt,
           [this](float v) { controller.setMasterRoomDb(v); },
           [this](juce::String busId) { controller.setRoomBusRouting(busId); },
           [this] { controller.toggleRoomMute(); },
           [this] { controller.toggleRoomSolo(); },
           [](float) {},
           DrumEngine::meterKeyFor({}, MicChannelDef { {}, {}, MicRole::Room, 1, 0.0f }),
           kit.roomSwapLR,
           [this] { controller.toggleRoomSwap(); });

    resized();
    repaint();
}

void GlobalMixerCard::paint(juce::Graphics& g)
{
    paintPanelBackground(g, getLocalBounds().toFloat(), 4.0f);

    g.setColour(Colours::line);
    for (int i = 1; i < rows.size(); ++i)
        g.drawVerticalLine(i * stripWidth, 12.0f, (float) getHeight() - 12.0f);

    constexpr float minDb = -60.0f, maxDb = 6.0f;
    auto yForDb = [](float db, juce::Rectangle<int> faderBounds)
    {
        const float frac = juce::jlimit(0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        return (float) faderBounds.getBottom() - frac * (float) faderBounds.getHeight();
    };

    for (auto* row : rows)
    {
        const auto faderBounds = row->fader.getBounds();

        // dB scale: a short tick flush against the fader, with the number
        // label to its left so the two never overlap.
        g.setFont(DrumVaultLookAndFeel::numericFont(8.0f));
        constexpr float tickLen = 5.0f;
        for (float db : { 6.0f, 0.0f, -3.0f, -6.0f, -12.0f, -24.0f, -40.0f, -60.0f })
        {
            const float y = yForDb(db, faderBounds);
            const bool major = db == 0.0f;

            g.setColour(major ? juce::Colour(0xff8a97a1) : juce::Colour(0xff45525b));
            g.drawHorizontalLine((int) y, (float) row->scaleBounds.getRight() - tickLen, (float) row->scaleBounds.getRight());

            g.setColour(major ? juce::Colour(0xffaeb8bf) : juce::Colour(0xff69757d));
            const juce::String label = db > 0.0f ? "+" + juce::String((int) db) : juce::String((int) db);
            const juce::Rectangle<float> textArea((float) row->scaleBounds.getX(), y - 5.0f,
                                                    (float) row->scaleBounds.getWidth() - tickLen - 3.0f, 10.0f);
            g.drawText(label, textArea, juce::Justification::centredRight);
        }

        // Output meter: post-fader peak of THIS channel's own hits, not the
        // bus it happens to be routed to (multiple channels can share a bus).
        const float peakLinear = controller.getProcessor().getChannelPeak(row->meterKey);
        const float peakDb = peakLinear <= 0.00001f ? minDb : juce::Decibels::gainToDecibels(peakLinear);

        g.setColour(juce::Colour(0xff10161a));
        g.fillRoundedRectangle(row->meterBounds.toFloat(), 2.0f);

        const float meterTopY = yForDb(juce::jlimit(minDb, maxDb, peakDb), row->meterBounds);
        const juce::Rectangle<float> filled(row->meterBounds.getX(), meterTopY,
                                             (float) row->meterBounds.getWidth(),
                                             (float) row->meterBounds.getBottom() - meterTopY);
        g.setGradientFill(juce::ColourGradient(Colours::green, 0.0f, (float) row->meterBounds.getBottom(),
                                                Colours::red, 0.0f, (float) row->meterBounds.getY(), false));
        g.fillRoundedRectangle(filled, 2.0f);
    }
}

void GlobalMixerCard::resized()
{
    auto area = getLocalBounds().reduced(0, 12);
    int x = area.getX();

    for (auto* row : rows)
    {
        auto strip = juce::Rectangle<int>(x, area.getY(), stripWidth, area.getHeight()).reduced(10, 0);

        row->nameLabel.setBounds(strip.removeFromTop(28));
        strip.removeFromTop(4);

        auto msRow = strip.removeFromTop(20);
        row->muteButton.setBounds(msRow.removeFromLeft((msRow.getWidth() - 4) / 2));
        msRow.removeFromLeft(4);
        row->soloButton.setBounds(msRow);
        strip.removeFromTop(6);

        // Reserved slot strip — currently just the L/R swap toggle on the
        // pooled Overheads/Room rows, left empty (but laid out) elsewhere so
        // future per-channel extras land in the same place on every strip.
        row->slotBounds = strip.removeFromTop(20);
        strip.removeFromTop(6);
        if (row->hasSwapControl)
            row->swapButton.setBounds(row->slotBounds.withSizeKeepingCentre(28, 20));

        row->routeBox.setBounds(strip.removeFromBottom(24));
        strip.removeFromBottom(6);
        if (row->hasPanControl)
        {
            row->panValueLabel.setBounds(strip.removeFromBottom(14));
            row->pan.setBounds(strip.removeFromBottom(20));
            strip.removeFromBottom(4);
        }
        row->valueLabel.setBounds(strip.removeFromBottom(16));
        strip.removeFromBottom(6);

        // What's left is the fader zone: dB scale ticks | fader | output meter.
        row->scaleBounds = strip.removeFromLeft(24);
        strip.removeFromLeft(4);
        row->meterBounds = strip.removeFromRight(7);
        strip.removeFromRight(4);
        row->fader.setBounds(strip);

        x += stripWidth;
    }
}

int GlobalMixerCard::getPreferredWidth() const
{
    return rows.size() * stripWidth;
}

// ----------------------------------------------------------- MasterStripPanel

MasterStripPanel::MasterStripPanel(KitEditController& controllerToUse)
    : controller(controllerToUse)
{
    controller.addListener(this);
    refresh();
    startTimerHz(30);
}

MasterStripPanel::~MasterStripPanel()
{
    controller.removeListener(this);
}

void MasterStripPanel::refresh()
{
    auto& kit = controller.getWorkingKit();

    nameLabel.setText("Master", juce::dontSendNotification);
    nameLabel.setFont(DrumVaultLookAndFeel::bodyFont(10.0f));
    nameLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(nameLabel);

    fader.setSliderStyle(juce::Slider::LinearVertical);
    fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    fader.setRange(-60.0, 6.0, 0.1);
    fader.setValue(kit.masterDb, juce::dontSendNotification);
    fader.setScrollWheelEnabled(false);
    fader.onValueChange = [this]
    {
        const float v = (float) fader.getValue();
        valueLabel.setText(juce::String(v, 1) + " dB", juce::dontSendNotification);
        controller.setMasterVolumeDb(v);
    };
    addAndMakeVisible(fader);

    valueLabel.setJustificationType(juce::Justification::centred);
    valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc8d0d6));
    valueLabel.setFont(DrumVaultLookAndFeel::numericFont(10.0f));
    valueLabel.setText(juce::String(kit.masterDb, 1) + " dB", juce::dontSendNotification);
    makeDbLabelEditable(valueLabel, fader);
    addAndMakeVisible(valueLabel);

    resized();
}

void MasterStripPanel::paint(juce::Graphics& g)
{
    paintPanelBackground(g, getLocalBounds().toFloat(), 4.0f);

    constexpr float minDb = -60.0f, maxDb = 6.0f;
    auto yForDb = [](float db, juce::Rectangle<int> faderBounds)
    {
        const float frac = juce::jlimit(0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        return (float) faderBounds.getBottom() - frac * (float) faderBounds.getHeight();
    };

    const auto faderBounds = fader.getBounds();

    g.setFont(DrumVaultLookAndFeel::numericFont(8.0f));
    constexpr float tickLen = 5.0f;
    for (float db : { 6.0f, 0.0f, -3.0f, -6.0f, -12.0f, -24.0f, -40.0f, -60.0f })
    {
        const float y = yForDb(db, faderBounds);
        const bool major = db == 0.0f;

        g.setColour(major ? juce::Colour(0xff8a97a1) : juce::Colour(0xff45525b));
        g.drawHorizontalLine((int) y, (float) scaleBounds.getRight() - tickLen, (float) scaleBounds.getRight());

        g.setColour(major ? juce::Colour(0xffaeb8bf) : juce::Colour(0xff69757d));
        const juce::String label = db > 0.0f ? "+" + juce::String((int) db) : juce::String((int) db);
        const juce::Rectangle<float> textArea((float) scaleBounds.getX(), y - 5.0f,
                                                (float) scaleBounds.getWidth() - tickLen - 3.0f, 10.0f);
        g.drawText(label, textArea, juce::Justification::centredRight);
    }

    // Post-master-fader output peak, same reading MeterCard shows.
    const float peakLinear = juce::jmax(controller.getProcessor().getMasterPeakL(),
                                         controller.getProcessor().getMasterPeakR());
    const float peakDb = peakLinear <= 0.00001f ? minDb : juce::Decibels::gainToDecibels(peakLinear);

    g.setColour(juce::Colour(0xff10161a));
    g.fillRoundedRectangle(meterBounds.toFloat(), 2.0f);

    const float meterTopY = yForDb(juce::jlimit(minDb, maxDb, peakDb), meterBounds);
    const juce::Rectangle<float> filled(meterBounds.getX(), meterTopY,
                                         (float) meterBounds.getWidth(),
                                         (float) meterBounds.getBottom() - meterTopY);
    g.setGradientFill(juce::ColourGradient(Colours::green, 0.0f, (float) meterBounds.getBottom(),
                                            Colours::red, 0.0f, (float) meterBounds.getY(), false));
    g.fillRoundedRectangle(filled, 2.0f);
}

void MasterStripPanel::resized()
{
    auto strip = getLocalBounds().reduced(10, 12);

    nameLabel.setBounds(strip.removeFromTop(28));
    strip.removeFromTop(4 + 20 + 6 + 20 + 6); // mute/solo row + slot row, kept blank to match a Row's geometry

    // Blank space matching a Row's route box, so the fader zone below lines
    // up vertically with every channel strip's.
    strip.removeFromBottom(24 + 6);

    valueLabel.setBounds(strip.removeFromBottom(16));
    strip.removeFromBottom(6);

    scaleBounds = strip.removeFromLeft(24);
    strip.removeFromLeft(4);
    meterBounds = strip.removeFromRight(7);
    strip.removeFromRight(4);
    fader.setBounds(strip);
}

// -------------------------------------------------------------- GlobalMixerView

GlobalMixerView::GlobalMixerView(KitEditController& controllerToUse)
    : globalMixerCard(controllerToUse), masterStripPanel(controllerToUse)
{
    heading.setText("GLOBAL MIXER", juce::dontSendNotification);
    heading.setFont(DrumVaultLookAndFeel::displayFont(16.0f));
    addAndMakeVisible(heading);

    hintLabel.setText("Output routing to your DAW's mixer", juce::dontSendNotification);
    hintLabel.setColour(juce::Label::textColourId, Colours::muted);
    hintLabel.setFont(DrumVaultLookAndFeel::bodyFont(11.0f));
    hintLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(hintLabel);

    viewport.setViewedComponent(&globalMixerCard, false);
    viewport.setScrollBarsShown(false, true);
    addAndMakeVisible(viewport);

    // Pinned outside the scrollable viewport so it's always reachable,
    // regardless of horizontal scroll position.
    addAndMakeVisible(masterStripPanel);
}

void GlobalMixerView::paint(juce::Graphics&) {}

void GlobalMixerView::resized()
{
    auto area = getLocalBounds().reduced(2);
    auto headRow = area.removeFromTop(38).reduced(16, 0);
    heading.setBounds(headRow.removeFromLeft(200));
    hintLabel.setBounds(headRow);

    auto contentArea = area.reduced(14, 8);
    masterStripPanel.setBounds(contentArea.removeFromRight(MasterStripPanel::preferredWidth));
    contentArea.removeFromRight(10);

    viewport.setBounds(contentArea);
    const int h = juce::jmax(100, viewport.getHeight() - viewport.getScrollBarThickness());
    const int w = juce::jmax(viewport.getWidth(), globalMixerCard.getPreferredWidth());
    globalMixerCard.setBounds(0, 0, w, h);
}

// ------------------------------------------------------- VelocityThresholdsCard

VelocityThresholdsCard::VelocityThresholdsCard(KitEditController& controllerToUse)
    : controller(controllerToUse)
{
    titleLabel.setText("VELOCITY THRESHOLDS", juce::dontSendNotification);
    titleLabel.setFont(DrumVaultLookAndFeel::displayFont(12.0f));
    addAndMakeVisible(titleLabel);

    resetButton.onClick = [this] { controller.resetVelocityThresholds(); };
    addAndMakeVisible(resetButton);

    layerCountFieldLabel.setText("VELOCITY LAYERS", juce::dontSendNotification);
    layerCountFieldLabel.setColour(juce::Label::textColourId, Colours::muted);
    layerCountFieldLabel.setFont(DrumVaultLookAndFeel::displayFont(9.0f));
    addAndMakeVisible(layerCountFieldLabel);

    layerCountBox.addItem("3 layers", 3);
    layerCountBox.addItem("4 layers", 4);
    layerCountBox.addItem("5 layers", 5);
    layerCountBox.onChange = [this] { controller.setVelocityLayerCount(layerCountBox.getSelectedId()); };
    addAndMakeVisible(layerCountBox);

    infoText.setText("Changes apply to all instruments. Samples are reassigned to the nearest layer.", juce::dontSendNotification);
    infoText.setColour(juce::Label::textColourId, Colours::muted);
    infoText.setFont(DrumVaultLookAndFeel::bodyFont(9.5f));
    infoText.setJustificationType(juce::Justification::topLeft);
    infoText.setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(infoText);

    orphanedButton.onClick = [this]
    {
        juce::NativeMessageBox::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::InfoIcon)
                .withTitle("Orphaned samples")
                .withMessage(juce::String(controller.getOrphanedSampleCount())
                             + " sample(s) remain safe when a velocity layer is removed by shrinking the layer "
                               "count. Reassign them from the instrument editor once a matching layer exists again.")
                .withButton("Got it"),
            nullptr);
    };
    addAndMakeVisible(orphanedButton);

    controller.addListener(this);
    refresh();
}

VelocityThresholdsCard::~VelocityThresholdsCard()
{
    controller.removeListener(this);
}

void VelocityThresholdsCard::refresh()
{
    auto& config = controller.getWorkingKit().velocityConfig;
    layerCountBox.setSelectedId(config.getLayerCount(), juce::dontSendNotification);

    if (rows.size() != (int) config.thresholds.size())
    {
        rebuildThresholdRows();
    }
    else
    {
        for (int i = 0; i < rows.size(); ++i)
        {
            if (rows[i]->slider.isMouseButtonDown())
                continue;

            const int lo = i > 0 ? config.thresholds[(size_t) i - 1] + 1 : 1;
            const int hi = i < (int) config.thresholds.size() - 1 ? config.thresholds[(size_t) i + 1] - 1 : 126;
            rows[i]->slider.setRange(lo, hi, 1.0);
            rows[i]->slider.setValue(config.thresholds[(size_t) i], juce::dontSendNotification);
            rows[i]->valueLabel.setText(juce::String(config.thresholds[(size_t) i]), juce::dontSendNotification);
        }
    }

    orphanedButton.setButtonText("VIEW ORPHANED SAMPLES (" + juce::String(controller.getOrphanedSampleCount()) + ")");
    repaint();
}

void VelocityThresholdsCard::rebuildThresholdRows()
{
    rows.clear();
    auto& config = controller.getWorkingKit().velocityConfig;

    for (int i = 0; i < (int) config.thresholds.size(); ++i)
    {
        auto* row = rows.add(new ThresholdRow());

        row->nameLabel.setText("THRESHOLD " + juce::String(i + 1), juce::dontSendNotification);
        row->nameLabel.setColour(juce::Label::textColourId, Colours::muted);
        row->nameLabel.setFont(DrumVaultLookAndFeel::bodyFont(9.0f));
        addAndMakeVisible(row->nameLabel);

        row->valueLabel.setJustificationType(juce::Justification::centred);
        row->valueLabel.setFont(DrumVaultLookAndFeel::numericFont(10.0f));
        row->valueLabel.setText(juce::String(config.thresholds[(size_t) i]), juce::dontSendNotification);
        addAndMakeVisible(row->valueLabel);

        row->slider.setSliderStyle(juce::Slider::LinearHorizontal);
        row->slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        const int lo = i > 0 ? config.thresholds[(size_t) i - 1] + 1 : 1;
        const int hi = i < (int) config.thresholds.size() - 1 ? config.thresholds[(size_t) i + 1] - 1 : 126;
        row->slider.setRange(lo, hi, 1.0);
        row->slider.setValue(config.thresholds[(size_t) i], juce::dontSendNotification);
        row->slider.setScrollWheelEnabled(false);

        const int index = i;
        row->slider.onValueChange = [this, index]
        {
            const int v = (int) rows[index]->slider.getValue();
            rows[index]->valueLabel.setText(juce::String(v), juce::dontSendNotification);
            controller.setThreshold(index, v);
        };
        addAndMakeVisible(row->slider);
    }

    resized();
}

void VelocityThresholdsCard::paint(juce::Graphics& g)
{
    paintPanelBackground(g, getLocalBounds().toFloat(), 4.0f);

    auto& config = controller.getWorkingKit().velocityConfig;
    std::vector<std::pair<int, int>> ranges;
    int start = 0;
    for (auto t : config.thresholds) { ranges.push_back({ start, t }); start = t + 1; }
    ranges.push_back({ start, 127 });

    static const juce::Colour segColours[] = { juce::Colour(0xff4f82ba), juce::Colour(0xff64a952),
                                                juce::Colour(0xffd0aa3e), juce::Colour(0xffbd4549) };

    if (stripBounds.getWidth() > 0)
    {
        int x = stripBounds.getX();
        constexpr int total = 128;
        for (int i = 0; i < (int) ranges.size(); ++i)
        {
            const int width = juce::roundToInt((double) (ranges[(size_t) i].second - ranges[(size_t) i].first + 1)
                                                 / total * stripBounds.getWidth());
            const juce::Rectangle<int> seg(x, stripBounds.getY(), juce::jmax(2, width - 2), 15);
            g.setColour(segColours[juce::jmin(i, 3)]);
            g.fillRoundedRectangle(seg.toFloat(), 2.0f);

            g.setColour(Colours::muted);
            g.setFont(DrumVaultLookAndFeel::bodyFont(7.5f));
            g.drawText(juce::String(ranges[(size_t) i].first) + "-" + juce::String(ranges[(size_t) i].second),
                       x, stripBounds.getY() + 18, width, 10, juce::Justification::centredLeft);

            x += width;
        }
    }
}

void VelocityThresholdsCard::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto titleRow = area.removeFromTop(20);
    titleLabel.setBounds(titleRow.removeFromLeft(titleRow.getWidth() - 60));
    resetButton.setBounds(titleRow);
    area.removeFromTop(8);

    layerCountFieldLabel.setBounds(area.removeFromTop(12));
    layerCountBox.setBounds(area.removeFromTop(29));
    area.removeFromTop(13);

    stripBounds = area.removeFromTop(28);
    area.removeFromTop(11);

    for (auto* row : rows)
    {
        auto rowArea = area.removeFromTop(31);
        row->nameLabel.setBounds(rowArea.removeFromLeft(67));
        row->valueLabel.setBounds(rowArea.removeFromLeft(35));
        rowArea.removeFromLeft(7);
        row->slider.setBounds(rowArea);
    }

    area.removeFromTop(10);
    infoText.setBounds(area.removeFromTop(50));
    area.removeFromTop(10);
    orphanedButton.setBounds(area.removeFromTop(32));
}

int VelocityThresholdsCard::getPreferredHeight() const
{
    return 250 + rows.size() * 31;
}

// ------------------------------------------------------------------ MasterCard

MasterCard::MasterCard(KitEditController& controllerToUse)
{
    titleLabel.setText("MASTER", juce::dontSendNotification);
    titleLabel.setFont(DrumVaultLookAndFeel::displayFont(12.0f));
    addAndMakeVisible(titleLabel);

    outputLabel.setJustificationType(juce::Justification::centredRight);
    outputLabel.setFont(DrumVaultLookAndFeel::numericFont(11.0f));
    addAndMakeVisible(outputLabel);

    auto& kit = controllerToUse.getWorkingKit();

    setupRow(volumeRow, "VOLUME", -60.0, 6.0, 0.1, kit.masterDb,
             [this, &controllerToUse](float v)
             {
                 controllerToUse.setMasterVolumeDb(v);
                 outputLabel.setText(juce::String(v, 1) + " dB", juce::dontSendNotification);
             }, "dB");

    setupRow(overheadRow, "OVERHEADS", -24.0, 6.0, 0.5, kit.masterOverheadDb,
             [&controllerToUse](float v) { controllerToUse.setMasterOverheadDb(v); }, "dB");

    setupRow(roomRow, "ROOM", -24.0, 6.0, 0.5, kit.masterRoomDb,
             [&controllerToUse](float v) { controllerToUse.setMasterRoomDb(v); }, "dB");

    setupRow(tuneRow, "TUNE", -12.0, 12.0, 1.0, kit.masterTuneSemitones,
             [&controllerToUse](float v) { controllerToUse.setMasterTuneSemitones(v); }, "st");

    outputLabel.setText(juce::String(kit.masterDb, 1) + " dB", juce::dontSendNotification);
}

void MasterCard::setupRow(Row& row, const juce::String& name, double lo, double hi, double step, double initial,
                           std::function<void(float)> onChange, const juce::String& unit)
{
    row.label.setText(name, juce::dontSendNotification);
    row.label.setColour(juce::Label::textColourId, Colours::muted);
    row.label.setFont(DrumVaultLookAndFeel::bodyFont(9.0f));
    addAndMakeVisible(row.label);

    row.slider.setSliderStyle(juce::Slider::LinearHorizontal);
    row.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    row.slider.setRange(lo, hi, step);
    row.slider.setValue(initial, juce::dontSendNotification);
    row.slider.setResetEnabled(unit != "st");
    row.slider.setScrollWheelEnabled(false);
    addAndMakeVisible(row.slider);

    row.value.setJustificationType(juce::Justification::centredRight);
    row.value.setColour(juce::Label::textColourId, juce::Colour(0xffc8d0d6));
    row.value.setFont(DrumVaultLookAndFeel::numericFont(10.0f));
    const int decimals = unit == "st" ? 0 : 1;
    row.value.setText(juce::String(initial, decimals) + " " + unit, juce::dontSendNotification);
    if (unit == "dB")
        makeDbLabelEditable(row.value, row.slider);
    addAndMakeVisible(row.value);

    row.slider.onValueChange = [&row, onChange, decimals, unit]
    {
        const float v = (float) row.slider.getValue();
        row.value.setText(juce::String(v, decimals) + " " + unit, juce::dontSendNotification);
        onChange(v);
    };
}

void MasterCard::paint(juce::Graphics& g)
{
    paintPanelBackground(g, getLocalBounds().toFloat(), 4.0f);
}

void MasterCard::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto titleRow = area.removeFromTop(20);
    titleLabel.setBounds(titleRow.removeFromLeft(100));
    outputLabel.setBounds(titleRow);
    area.removeFromTop(10);

    auto layoutRow = [&area](Row& row)
    {
        auto r = area.removeFromTop(34);
        row.label.setBounds(r.removeFromLeft(70));
        row.value.setBounds(r.removeFromRight(38));
        r.removeFromRight(7);
        row.slider.setBounds(r);
    };
    layoutRow(volumeRow);
    layoutRow(overheadRow);
    layoutRow(roomRow);
    layoutRow(tuneRow);
}

// ------------------------------------------------------------------- MeterCard

MeterCard::MeterCard(KitEditController& controllerToUse)
    : controller(controllerToUse)
{
    startTimerHz(30);
}

float MeterCard::gainToDb(float g)
{
    return g <= 0.00001f ? -100.0f : juce::Decibels::gainToDecibels(g);
}

void MeterCard::timerCallback()
{
    displayedL = controller.getProcessor().getMasterPeakL();
    displayedR = controller.getProcessor().getMasterPeakR();
    peakDbL = juce::jmax(peakDbL - 0.5f, gainToDb(displayedL));
    peakDbR = juce::jmax(peakDbR - 0.5f, gainToDb(displayedR));
    repaint();
}

void MeterCard::paint(juce::Graphics& g)
{
    paintPanelBackground(g, getLocalBounds().toFloat(), 4.0f);

    auto area = getLocalBounds().reduced(12);
    g.setColour(Colours::text);
    g.setFont(DrumVaultLookAndFeel::displayFont(12.0f));
    g.drawText("MASTER OUTPUT", area.removeFromTop(20), juce::Justification::centredLeft);
    area.removeFromTop(8);

    auto meterArea = area.removeFromTop(78);
    meterLBounds = meterArea.removeFromLeft(24);
    meterArea.removeFromLeft(4);
    meterRBounds = meterArea.removeFromLeft(24);

    auto drawMeter = [&g](juce::Rectangle<int> bounds, float level)
    {
        g.setColour(juce::Colour(0xff10161a));
        g.fillRoundedRectangle(bounds.toFloat(), 2.0f);

        const float db = juce::jlimit(-60.0f, 0.0f, gainToDb(level));
        const float frac = (db + 60.0f) / 60.0f;
        const int filledH = (int) (bounds.getHeight() * frac);
        const juce::Rectangle<int> filled(bounds.getX(), bounds.getBottom() - filledH, bounds.getWidth(), filledH);

        g.setGradientFill(juce::ColourGradient(Colours::green, 0.0f, (float) bounds.getBottom(),
                                                Colours::red, 0.0f, (float) bounds.getY(), false));
        g.fillRoundedRectangle(filled.toFloat(), 2.0f);
    };

    drawMeter(meterLBounds, displayedL);
    drawMeter(meterRBounds, displayedR);

    area.removeFromTop(8);
    auto readout = area.removeFromTop(20);
    g.setColour(Colours::text);
    g.setFont(DrumVaultLookAndFeel::numericFont(10.0f));
    g.drawText("L " + (peakDbL <= -99.0f ? juce::String("-inf") : juce::String(peakDbL, 1)),
               readout.removeFromLeft(readout.getWidth() / 2), juce::Justification::centredLeft);
    g.drawText("R " + (peakDbR <= -99.0f ? juce::String("-inf") : juce::String(peakDbR, 1)),
               readout, juce::Justification::centredLeft);
}

void MeterCard::resized() {}

// ---------------------------------------------------------------- MidiMapCard

MidiMapCard::MidiMapCard(KitEditController& controllerToUse, ToastOverlay& toastsToUse, ModalHost& modalHostToUse)
    : controller(controllerToUse), toasts(toastsToUse), modalHost(modalHostToUse)
{
    titleLabel.setText("MIDI MAP", juce::dontSendNotification);
    titleLabel.setFont(DrumVaultLookAndFeel::displayFont(12.0f));
    addAndMakeVisible(titleLabel);

    exportButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser>("Export MIDI map", juce::File(), "*.json");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File {})
                    return;
                if (! file.hasFileExtension("json"))
                    file = file.withFileExtension("json");

                juce::Array<juce::var> rows;
                for (auto& inst : controller.getWorkingKit().instruments)
                {
                    for (auto& articulation : inst.articulations)
                    {
                        auto obj = std::make_unique<juce::DynamicObject>();
                        obj->setProperty("instrumentId", inst.id);
                        obj->setProperty("articulationId", articulation.id);
                        obj->setProperty("name", inst.name);
                        obj->setProperty("articulation", articulation.name);
                        obj->setProperty("midiNote", articulation.midiNote);
                        rows.add(juce::var(obj.release()));
                    }
                }
                file.replaceWithText(juce::JSON::toString(juce::var(rows)));
                toasts.show("MIDI map exported", file.getFileName());
            });
    };
    addAndMakeVisible(exportButton);

    importButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser>("Import MIDI map", juce::File(), "*.json");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (! file.existsAsFile())
                    return;

                auto* rows = juce::JSON::parse(file.loadFileAsString()).getArray();
                if (rows == nullptr)
                {
                    toasts.show("MIDI map import failed", "Not a valid MIDI map file");
                    return;
                }

                int applied = 0;
                for (auto& rowVar : *rows)
                {
                    const auto instrumentId = rowVar.getProperty("instrumentId", {}).toString();
                    const auto articulationId = rowVar.getProperty("articulationId", {}).toString();
                    const int midiNote = (int) rowVar.getProperty("midiNote", -1);
                    if (instrumentId.isEmpty() || articulationId.isEmpty())
                        continue;

                    // force=true: an imported map has no internal duplicates
                    // by construction (it came from export), so it should
                    // always apply cleanly even if it collides with notes
                    // the kit happened to have before the import started.
                    if (controller.setArticulationMidiNote(instrumentId, articulationId, midiNote, true)
                        == Kit::SetNoteResult::Ok)
                        ++applied;
                }

                toasts.show("MIDI map imported",
                            juce::String(applied) + " of " + juce::String(rows->size()) + " note(s) applied");
            });
    };
    addAndMakeVisible(importButton);

    editButton.onClick = [this]
    {
        modalHost.show(std::make_unique<InfoDialog>(modalHost, "MIDI MAP", "Editable mapping",
            "Select an instrument in the Kit Overview, then use its MIDI Learn button or the MIDI note dropdown "
            "in the Instrument Editor to change its trigger note."), 460, 240);
    };
    addAndMakeVisible(editButton);

    controller.addListener(this);
}

MidiMapCard::~MidiMapCard()
{
    controller.removeListener(this);
}

int MidiMapCard::getPreferredHeight() const
{
    int articulationCount = 0;
    for (auto& instrument : controller.getWorkingKit().instruments)
        articulationCount += (int) instrument.articulations.size();
    return 76 + articulationCount * 24 + 44;
}

void MidiMapCard::paint(juce::Graphics& g)
{
    paintPanelBackground(g, getLocalBounds().toFloat(), 4.0f);

    auto area = getLocalBounds().reduced(12);
    area.removeFromTop(32);

    auto& instruments = controller.getWorkingKit().instruments;
    int articulationCount = 0;
    for (auto& instrument : instruments)
        articulationCount += (int) instrument.articulations.size();
    const juce::Rectangle<int> listBounds = area.removeFromTop(articulationCount * 24);

    g.setColour(juce::Colour(0xff2d383f));
    g.drawRect(listBounds);

    int y = listBounds.getY();
    for (auto& inst : instruments)
    {
        for (auto& articulation : inst.articulations)
        {
            auto row = juce::Rectangle<int>(listBounds.getX(), y, listBounds.getWidth(), 24).reduced(7, 0);

            g.setColour(Colours::blueBright);
            g.setFont(DrumVaultLookAndFeel::numericFont(10.0f));
            g.drawText(articulation.midiNote >= 0 ? midiNoteToName(articulation.midiNote) : "-",
                       row.removeFromLeft(35), juce::Justification::centredLeft);

            g.setColour(juce::Colour(0xff53606a));
            g.setFont(DrumVaultLookAndFeel::bodyFont(8.5f));
            g.drawText(inst.category == InstrumentCategory::Cymbal ? "CYM" : "BODY",
                       row.removeFromRight(35), juce::Justification::centredRight);

            g.setColour(Colours::text);
            g.setFont(DrumVaultLookAndFeel::bodyFont(9.5f));
            const auto display = inst.name + (inst.articulations.size() > 1 ? " - " + articulation.name : juce::String());
            g.drawText(display, row, juce::Justification::centredLeft);

            g.setColour(juce::Colour(0xff283138));
            g.drawHorizontalLine(y + 24, (float) listBounds.getX(), (float) listBounds.getRight());
            y += 24;
        }
    }
}

void MidiMapCard::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto titleRow = area.removeFromTop(20);
    titleLabel.setBounds(titleRow.removeFromLeft(100));
    importButton.setBounds(titleRow.removeFromLeft(70));
    titleRow.removeFromLeft(6);
    exportButton.setBounds(titleRow);
    area.removeFromTop(8);

    int articulationCount = 0;
    for (auto& instrument : controller.getWorkingKit().instruments)
        articulationCount += (int) instrument.articulations.size();
    area.removeFromTop(articulationCount * 24 + 4);
    editButton.setBounds(area.removeFromTop(32));
}

// ------------------------------------------------------------------- SidePanel

SidePanel::SidePanel(KitEditController& controllerToUse, ToastOverlay& toastsToUse, ModalHost& modalHostToUse)
    : velocityCard(controllerToUse), masterCard(controllerToUse),
      meterCard(controllerToUse), midiMapCard(controllerToUse, toastsToUse, modalHostToUse)
{
    eyebrow.setText("GLOBAL SETTINGS", juce::dontSendNotification);
    eyebrow.setFont(DrumVaultLookAndFeel::displayFont(14.0f));
    addAndMakeVisible(eyebrow);

    tinyStatus.setText("Applied to all instruments", juce::dontSendNotification);
    tinyStatus.setColour(juce::Label::textColourId, Colours::muted);
    tinyStatus.setFont(DrumVaultLookAndFeel::bodyFont(9.0f));
    addAndMakeVisible(tinyStatus);

    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    content.addAndMakeVisible(velocityCard);
    content.addAndMakeVisible(masterCard);
    content.addAndMakeVisible(meterCard);
    content.addAndMakeVisible(midiMapCard);
}

void SidePanel::paint(juce::Graphics&) {}

void SidePanel::resized()
{
    auto area = getLocalBounds().reduced(14, 14);
    auto headerRow = area.removeFromTop(48);
    eyebrow.setBounds(headerRow.removeFromTop(20));
    tinyStatus.setBounds(headerRow);

    viewport.setBounds(area);

    const int w = juce::jmax(100, viewport.getWidth() - viewport.getScrollBarThickness());
    int y = 0;
    constexpr int gap = 11;

    velocityCard.setBounds(0, y, w, velocityCard.getPreferredHeight());
    y += velocityCard.getPreferredHeight() + gap;

    masterCard.setBounds(0, y, w, MasterCard::preferredHeight);
    y += MasterCard::preferredHeight + gap;

    meterCard.setBounds(0, y, w, MeterCard::preferredHeight);
    y += MeterCard::preferredHeight + gap;

    midiMapCard.setBounds(0, y, w, midiMapCard.getPreferredHeight());
    y += midiMapCard.getPreferredHeight();

    content.setSize(w, y);
}

} // namespace dsl::ui
