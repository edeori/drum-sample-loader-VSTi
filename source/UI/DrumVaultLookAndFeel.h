#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace dsl::ui
{

// Colour language lifted from mockup/styles.css (:root custom properties).
namespace Colours
{
    const juce::Colour bg          { 0xff070a0d };
    const juce::Colour panel       { 0xff11171c };
    const juce::Colour panel2      { 0xff151c22 };
    const juce::Colour line        { 0xff2a343d };
    const juce::Colour lineSoft    { 0xff202930 };
    const juce::Colour text        { 0xffedf2f6 };
    const juce::Colour muted       { 0xff84909a };
    const juce::Colour blue        { 0xff4b9cf6 };
    const juce::Colour blueBright  { 0xff72b9ff };
    const juce::Colour blueDark    { 0xff153d6d };
    const juce::Colour green       { 0xff71bd56 };
    const juce::Colour yellow      { 0xffe2b849 };
    const juce::Colour red         { 0xffd65759 };
}

// Shared "panel"/"side-card" chrome (border + subtle gradient), matching
// mockup/styles.css .panel / .side-card. Used by every top-level section.
void paintPanelBackground(juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius = 5.0f);

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

/** House look for the whole plugin editor: dark panels, blue accent, condensed
    display headings — matching mockup/styles.css as closely as native JUCE
    widget drawing reasonably allows.
*/
class DrumVaultLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DrumVaultLookAndFeel();

    static juce::Font displayFont(float height, bool bold = true);
    static juce::Font bodyFont(float height, bool bold = false);

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle, juce::Slider&) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
};

} // namespace dsl::ui
