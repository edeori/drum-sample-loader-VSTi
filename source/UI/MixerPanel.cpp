#include "MixerPanel.h"
#include <cmath>

namespace dsl::ui
{

// ------------------------------------------------------------ MixerChannelStrip

MixerChannelStrip::MixerChannelStrip(KitEditController& controllerToUse, juce::String micChannelIdToUse)
    : controller(controllerToUse), micChannelId(std::move(micChannelIdToUse))
{
    auto* inst = controller.getSelectedInstrument();
    const auto* micDef = inst != nullptr ? inst->findChannel(micChannelId) : nullptr;

    nameLabel.setText(micDef != nullptr && micDef->displayName.isNotEmpty() ? micDef->displayName : micChannelId,
                       juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setFont(DrumVaultLookAndFeel::bodyFont(11.0f));
    addAndMakeVisible(nameLabel);

    fader.setSliderStyle(juce::Slider::LinearVertical);
    fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    fader.setRange(-60.0, 6.0, 0.1);
    fader.setValue(inst != nullptr ? (double) inst->getChannelFaderDb(micChannelId) : 0.0, juce::dontSendNotification);
    fader.setScrollWheelEnabled(false);
    lastFaderValue = fader.getValue();
    fader.onValueChange = [this]
    {
        const double newValue = fader.getValue();
        const double delta = newValue - lastFaderValue;
        lastFaderValue = newValue;

        controller.setChannelFaderDb(micChannelId, (float) newValue);
        valueLabel.setText(juce::String(newValue, 1) + " dB", juce::dontSendNotification);

        if (linkPartner != nullptr && linkedFlagPtr != nullptr && *linkedFlagPtr)
            linkPartner->applyLinkedDelta(delta);
    };
    addAndMakeVisible(fader);

    valueLabel.setText(juce::String(fader.getValue(), 1) + " dB", juce::dontSendNotification);
    valueLabel.setJustificationType(juce::Justification::centred);
    valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd3d9));
    valueLabel.setFont(DrumVaultLookAndFeel::numericFont(10.5f));
    makeDbLabelEditable(valueLabel, fader);
    addAndMakeVisible(valueLabel);

    const float pan = micDef != nullptr ? micDef->pan : 0.0f;
    const auto panText = micDef != nullptr && micDef->numChannels == 2 ? juce::String("STEREO")
                       : pan <= -0.999f ? juce::String("PAN L100")
                       : pan >= 0.999f ? juce::String("PAN R100")
                       : juce::String("PAN C");
    panLabel.setText(panText, juce::dontSendNotification);
    panLabel.setJustificationType(juce::Justification::centred);
    panLabel.setColour(juce::Label::textColourId,
                       std::abs(pan) > 0.999f ? Colours::blueBright : Colours::muted);
    panLabel.setFont(DrumVaultLookAndFeel::numericFont(9.0f));
    addAndMakeVisible(panLabel);
}

void MixerChannelStrip::paint(juce::Graphics& g)
{
    g.setColour(Colours::line);
    g.drawVerticalLine(getWidth() - 1, 0.0f, (float) getHeight());
}

void MixerChannelStrip::resized()
{
    auto area = getLocalBounds().reduced(8, 10);
    nameLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(4);

    auto faderArea = area.removeFromTop(area.getHeight() - 34);
    fader.setBounds(faderArea.withSizeKeepingCentre(faderArea.getWidth(), faderArea.getHeight()));

    valueLabel.setBounds(area.removeFromTop(16));
    panLabel.setBounds(area.removeFromTop(16));
}

void MixerChannelStrip::applyLinkedDelta(double deltaDb)
{
    const double newValue = juce::jlimit(fader.getMinimum(), fader.getMaximum(), fader.getValue() + deltaDb);
    fader.setValue(newValue, juce::dontSendNotification);
    lastFaderValue = newValue;
    valueLabel.setText(juce::String(newValue, 1) + " dB", juce::dontSendNotification);
    controller.setChannelFaderDb(micChannelId, (float) newValue);
}

// ------------------------------------------------------------------ MixerPanel

MixerPanel::MixerPanel(KitEditController& controllerToUse)
    : controller(controllerToUse)
{
    heading.setText("CHANNEL MIX", juce::dontSendNotification);
    heading.setFont(DrumVaultLookAndFeel::displayFont(14.0f));
    addAndMakeVisible(heading);

    statusLabel.setText("MULTI-OUT ACTIVE", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff697781));
    statusLabel.setFont(DrumVaultLookAndFeel::displayFont(9.0f));
    statusLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(statusLabel);

    controller.addListener(this);
    rebuild();
}

MixerPanel::~MixerPanel()
{
    controller.removeListener(this);
}

void MixerPanel::kitEditorStateChanged() { rebuild(); }

void MixerPanel::rebuild()
{
    strips.clear();
    linkButtons.clear();
    linkButtonAfterIndex.clear();

    auto* inst = controller.getSelectedInstrument();
    if (inst == nullptr)
    {
        resized();
        return;
    }

    for (auto& mic : inst->micChannels)
        addAndMakeVisible(strips.add(new MixerChannelStrip(controller, mic.id)));

    // Wire up + add a link toggle for every adjacent mono L/R ambience pair
    // (OH_L/OH_R, Room_L/Room_R — see DefaultKitFactory::addAmbienceChannels).
    for (int i = 0; i + 1 < (int) inst->micChannels.size(); ++i)
    {
        auto& a = inst->micChannels[(size_t) i];
        auto& b = inst->micChannels[(size_t) i + 1];
        if (! isAmbienceRole(a.role) || a.role != b.role || a.numChannels != 1 || b.numChannels != 1)
            continue;
        if (! (a.id.endsWith("_L") && b.id.endsWith("_R")))
            continue;

        bool* linkedFlag = a.role == MicRole::Overhead ? &inst->ohFaderLinked : &inst->roomFaderLinked;
        strips[i]->setLinkPartner(strips[i + 1], linkedFlag);
        strips[i + 1]->setLinkPartner(strips[i], linkedFlag);

        const auto role = a.role;
        auto* button = linkButtons.add(new juce::TextButton(juce::String::charToString((juce::juce_wchar) 0x1f517)));
        button->setTooltip(*linkedFlag ? "Unlink L/R faders" : "Link L/R faders");
        button->setClickingTogglesState(true);
        button->setToggleState(*linkedFlag, juce::dontSendNotification);
        button->onClick = [this, role] { controller.toggleAmbienceFaderLink(role); };
        addAndMakeVisible(button);
        linkButtonAfterIndex.push_back(i);
    }

    resized();
}

void MixerPanel::paint(juce::Graphics& g)
{
    paintPanelBackground(g, getLocalBounds().toFloat());
}

void MixerPanel::resized()
{
    auto area = getLocalBounds();
    auto toolbar = area.removeFromTop(39).reduced(13, 0);
    heading.setBounds(toolbar.removeFromLeft(260));
    statusLabel.setBounds(toolbar);

    if (strips.isEmpty())
        return;

    const int stripW = area.getWidth() / strips.size();
    int x = area.getX();
    for (auto* strip : strips)
    {
        strip->setBounds(x, area.getY(), stripW, area.getHeight());
        x += stripW;
    }

    for (int i = 0; i < linkButtons.size(); ++i)
    {
        const int boundaryX = area.getX() + (linkButtonAfterIndex[(size_t) i] + 1) * stripW;
        linkButtons[i]->setBounds(boundaryX - 12, area.getY() + 8, 24, 20);
    }
}

} // namespace dsl::ui
