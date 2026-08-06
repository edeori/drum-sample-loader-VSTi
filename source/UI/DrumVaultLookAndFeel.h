#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace dsl::ui
{

// Colour language lifted from mockup/styles.css (:root custom properties).
namespace Colours
{
    const juce::Colour bg          { 0xff050609 };
    const juce::Colour panel       { 0xff0b0d12 };
    const juce::Colour panel2      { 0xff11141a };
    const juce::Colour line        { 0xff303640 };
    const juce::Colour lineSoft    { 0xff1c2027 };
    const juce::Colour text        { 0xffe8ebf1 };
    const juce::Colour muted       { 0xff79818e };
    const juce::Colour blue        { 0xff42d6ff };
    const juce::Colour blueBright  { 0xff78e5ff };
    const juce::Colour blueDark    { 0xff123946 };
    const juce::Colour violet      { 0xff8a43ff };
    const juce::Colour green       { 0xff6ee48d };
    const juce::Colour yellow      { 0xffffb521 };
    const juce::Colour red         { 0xffff4b54 };
}

juce::String withLetterSpacing(const juce::String& text);
void paintMetalTexture(juce::Graphics& g, juce::Rectangle<float> bounds, float opacity = 0.10f);
void paintHardwareScrew(juce::Graphics& g, juce::Point<float> centre, float radius);
juce::Image loadReadableMothLogo(int pixelHeight);

// Shared Moth Production recessed panel chrome. Used by every top-level
// section without changing any of their existing bounds or responsibilities.
void paintPanelBackground(juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius = 5.0f);

/** Makes a fader's existing "X.X dB" value label double-click editable:
    typing a number and pressing Enter (or clicking away) drives the slider
    to that value, clamped to its range. Reuses the slider's own
    onValueChange (already wired at every call site to reformat the label
    and push the change through the controller) rather than duplicating that
    commit path — this only re-drives the slider. */
void makeDbLabelEditable(juce::Label& label, juce::Slider& slider);

/** Slider used for audio gain controls. Double-click or Cmd/Ctrl-click resets
    it to 0 dB without beginning a drag. Non-fader sliders can disable this. */
class ResettableSlider : public juce::Slider
{
public:
    ResettableSlider() { setDoubleClickReturnValue(true, 0.0); }

    void setResetEnabled(bool shouldReset)
    {
        resetEnabled = shouldReset;
        setDoubleClickReturnValue(shouldReset, 0.0);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (resetEnabled && (event.mods.isCommandDown() || event.mods.isCtrlDown()))
        {
            setValue(0.0, juce::sendNotificationSync);
            return;
        }

        juce::Slider::mouseDown(event);
    }

private:
    bool resetEnabled = true;
};

/** Moth Production house look: black machined surfaces, cyan instrument
    accents and the shared Oxanium/Barlow Condensed/Rajdhani font stack. */
class DrumVaultLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DrumVaultLookAndFeel();

    static juce::Font displayFont(float height, bool bold = true);
    static juce::Font bodyFont(float height, bool bold = false);
    static juce::Font numericFont(float height);

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle, juce::Slider&) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

private:
    juce::Image buttonOffImage, buttonOnImage, sliderThumbImage;
};

} // namespace dsl::ui
