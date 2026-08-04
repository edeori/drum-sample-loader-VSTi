#include "DrumVaultLookAndFeel.h"

namespace dsl::ui
{

void paintPanelBackground(juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius)
{
    juce::ColourGradient grad(juce::Colour(0xff171e24).withAlpha(0.94f), bounds.getX(), bounds.getY(),
                               juce::Colour(0xff0d1216).withAlpha(0.96f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, cornerRadius);
    g.setColour(Colours::line);
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerRadius, 1.0f);
}

DrumVaultLookAndFeel::DrumVaultLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, Colours::bg);

    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222b32));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff245f9c));
    setColour(juce::TextButton::textColourOffId, Colours::text);
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);

    setColour(juce::Label::textColourId, Colours::text);

    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0c1216));
    setColour(juce::ComboBox::textColourId, Colours::text);
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff37424a));
    setColour(juce::ComboBox::arrowColourId, Colours::muted);

    setColour(juce::Slider::backgroundColourId, Colours::bg);
    setColour(juce::Slider::trackColourId, Colours::blue);
    setColour(juce::Slider::thumbColourId, juce::Colour(0xffdbe1e4));
    setColour(juce::Slider::textBoxTextColourId, Colours::text);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff090e11));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff2a343d));

    setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff090e11));
    setColour(juce::TextEditor::textColourId, Colours::text);
    setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff37434c));
    setColour(juce::TextEditor::focusedOutlineColourId, Colours::blue);

    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff11181d));
    setColour(juce::PopupMenu::textColourId, Colours::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff245f9c));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff38464f));
    setColour(juce::ScrollBar::trackColourId, Colours::bg);
}

juce::Font DrumVaultLookAndFeel::displayFont(float height, bool bold)
{
    auto opts = juce::FontOptions().withHeight(height);
    if (bold)
        opts = opts.withStyle("Bold");
    return juce::Font(opts).withHorizontalScale(0.94f).withExtraKerningFactor(0.01f);
}

juce::Font DrumVaultLookAndFeel::bodyFont(float height, bool bold)
{
    auto opts = juce::FontOptions().withHeight(height);
    if (bold)
        opts = opts.withStyle("Bold");
    return juce::Font(opts);
}

juce::Font DrumVaultLookAndFeel::getLabelFont(juce::Label&)
{
    return bodyFont(13.0f);
}

juce::Font DrumVaultLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return displayFont(juce::jmin(14.0f, (float) buttonHeight * 0.46f));
}

juce::Font DrumVaultLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return bodyFont(12.0f);
}

void DrumVaultLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                 bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat();
    const bool primary = button.getProperties().getWithDefault("primary", false);
    const bool danger = button.getProperties().getWithDefault("danger", false);
    const bool toggledOn = button.getToggleState();

    juce::Colour top, bottom, border;

    if (danger && isHighlighted)
    {
        top = juce::Colour(0xff8a3c3f);
        bottom = juce::Colour(0xff552527);
        border = juce::Colour(0xffa4484a);
    }
    else if (primary || toggledOn)
    {
        top = juce::Colour(0xff245f9c);
        bottom = juce::Colour(0xff143b65);
        border = juce::Colour(0xff377ccc);
        if (isHighlighted) { top = top.brighter(0.1f); bottom = bottom.brighter(0.1f); }
    }
    else
    {
        top = juce::Colour(0xff222b32);
        bottom = juce::Colour(0xff151c21);
        border = Colours::line;
        if (isHighlighted) { top = juce::Colour(0xff29343d); bottom = juce::Colour(0xff192229); border = juce::Colour(0xff4a5a66); }
    }

    if (isDown)
    {
        top = top.darker(0.15f);
        bottom = bottom.darker(0.15f);
    }

    juce::ColourGradient grad(top, bounds.getX(), bounds.getY(), bottom, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);
}

void DrumVaultLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float, float,
                                             const juce::Slider::SliderStyle style, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height);
    const bool vertical = style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical;

    if (vertical)
    {
        const float trackW = 4.0f;
        const juce::Rectangle<float> track(bounds.getCentreX() - trackW * 0.5f, bounds.getY(), trackW, bounds.getHeight());
        g.setColour(Colours::bg);
        g.fillRoundedRectangle(track, trackW * 0.5f);

        const juce::Rectangle<float> fill(track.getX(), sliderPos, trackW, track.getBottom() - sliderPos);
        g.setColour(Colours::blue.withAlpha(0.85f));
        g.fillRoundedRectangle(fill, trackW * 0.5f);

        const float thumbW = juce::jmin(20.0f, bounds.getWidth() * 0.85f);
        const float thumbH = 11.0f;
        const juce::Rectangle<float> thumb(bounds.getCentreX() - thumbW * 0.5f, sliderPos - thumbH * 0.5f, thumbW, thumbH);
        juce::ColourGradient tg(juce::Colour(0xffdbe1e4), thumb.getX(), thumb.getY(), juce::Colour(0xff616d75), thumb.getX(), thumb.getBottom(), false);
        tg.addColour(0.5, juce::Colour(0xff727e86));
        g.setGradientFill(tg);
        g.fillRoundedRectangle(thumb, 2.0f);
        g.setColour(juce::Colour(0xff9ba8b0));
        g.drawRoundedRectangle(thumb, 2.0f, 1.0f);
    }
    else
    {
        const float trackH = 4.0f;
        const juce::Rectangle<float> track(bounds.getX(), bounds.getCentreY() - trackH * 0.5f, bounds.getWidth(), trackH);
        g.setColour(Colours::bg);
        g.fillRoundedRectangle(track, trackH * 0.5f);

        const juce::Rectangle<float> fill(track.getX(), track.getY(), sliderPos - track.getX(), trackH);
        g.setColour(Colours::blue.withAlpha(0.85f));
        g.fillRoundedRectangle(fill, trackH * 0.5f);

        const float thumbD = 13.0f;
        const juce::Rectangle<float> thumb(sliderPos - thumbD * 0.5f, bounds.getCentreY() - thumbD * 0.5f, thumbD, thumbD);
        g.setColour(juce::Colour(0xffdbe1e4));
        g.fillEllipse(thumb);
        g.setColour(juce::Colour(0xff9ba8b0));
        g.drawEllipse(thumb, 1.0f);
    }
}

void DrumVaultLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                         int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height);
    g.setColour(juce::Colour(0xff0c1216));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(box.isPopupActive() || isButtonDown ? Colours::blue : juce::Colour(0xff37424a));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);

    auto arrowZone = bounds.removeFromRight(18.0f);
    juce::Path p;
    p.addTriangle(arrowZone.getCentreX() - 4.0f, arrowZone.getCentreY() - 2.0f,
                  arrowZone.getCentreX() + 4.0f, arrowZone.getCentreY() - 2.0f,
                  arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);
    g.setColour(Colours::muted);
    g.fillPath(p);
}

} // namespace dsl::ui
