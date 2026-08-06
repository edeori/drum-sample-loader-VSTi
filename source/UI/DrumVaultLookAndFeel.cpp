#include "DrumVaultLookAndFeel.h"
#include "SwarmFonts.h"
#include <BinaryData.h>

namespace dsl::ui
{
namespace
{
const juce::Rectangle<int> buttonOffInterior { 58, 66, 496, 92 };
const juce::Rectangle<int> buttonOnInterior  { 61, 67, 509, 91 };

juce::Image loadImage(const void* data, int size)
{
    return juce::ImageFileFormat::loadFrom(data, (size_t) size);
}

const juce::Image& metalTexture()
{
    static auto image = loadImage(BinaryData::mothmetaltexture_png, BinaryData::mothmetaltexture_pngSize);
    return image;
}

void drawFlatChrome(juce::Graphics& g, const juce::Image& image, juce::Rectangle<int> source,
                    juce::Rectangle<float> destination, float cornerRadius)
{
    if (! image.isValid() || destination.isEmpty())
        return;

    g.saveState();
    juce::Path clip;
    clip.addRoundedRectangle(destination, cornerRadius);
    g.reduceClipRegion(clip);
    g.drawImage(image.getClippedImage(source), destination);
    g.restoreState();
}
}

juce::String withLetterSpacing(const juce::String& text)
{
    juce::String result;
    for (int i = 0; i < text.length(); ++i)
    {
        result << text[i];
        if (i < text.length() - 1)
            result << " ";
    }
    return result;
}

void paintMetalTexture(juce::Graphics& g, juce::Rectangle<float> bounds, float opacity)
{
    const auto& image = metalTexture();
    if (! image.isValid() || bounds.isEmpty() || opacity <= 0.0f)
        return;

    auto source = image.getBounds().toFloat();
    const float destinationAspect = bounds.getWidth() / bounds.getHeight();
    const float sourceAspect = source.getWidth() / source.getHeight();
    if (sourceAspect > destinationAspect)
        source = source.withSizeKeepingCentre(source.getHeight() * destinationAspect, source.getHeight());
    else
        source = source.withSizeKeepingCentre(source.getWidth(), source.getWidth() / destinationAspect);

    g.setOpacity(opacity);
    g.drawImage(image.getClippedImage(source.toNearestInt()), bounds);
    g.setOpacity(1.0f);
}

void paintHardwareScrew(juce::Graphics& g, juce::Point<float> centre, float radius)
{
    auto body = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre);
    g.setColour(juce::Colours::black.withAlpha(0.72f));
    g.fillEllipse(body.expanded(1.5f));

    juce::ColourGradient face(juce::Colour(0xff4b5058), body.getTopLeft(),
                              juce::Colour(0xff090a0d), body.getBottomRight(), false);
    face.addColour(0.42, juce::Colour(0xff20242a));
    g.setGradientFill(face);
    g.fillEllipse(body);
    g.setColour(juce::Colour(0xff07080a));
    g.drawEllipse(body.reduced(0.5f), 1.0f);

    g.setColour(juce::Colour(0xff090a0c));
    g.drawLine(centre.x - radius * 0.43f, centre.y + radius * 0.28f,
               centre.x + radius * 0.43f, centre.y - radius * 0.28f, juce::jmax(1.0f, radius * 0.18f));
}

juce::Image loadReadableMothLogo(int pixelHeight)
{
    const auto raw = loadImage(BinaryData::moth_logo_png, BinaryData::moth_logo_pngSize);
    if (! raw.isValid())
        return {};

    const int width = juce::roundToInt((float) pixelHeight * (592.0f / 380.0f));
    const auto scaled = raw.rescaled(width, pixelHeight, juce::Graphics::highResamplingQuality)
                           .convertedToFormat(juce::Image::ARGB);
    juce::Image result(juce::Image::ARGB, width, pixelHeight, true);
    juce::Image::BitmapData source(scaled, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData destination(result, juce::Image::BitmapData::writeOnly);

    constexpr juce::uint8 threshold = 40;
    const int dilate = juce::jmax(1, pixelHeight / 60);
    for (int y = 0; y < destination.height; ++y)
        for (int x = 0; x < destination.width; ++x)
        {
            bool opaque = false;
            for (int dy = -dilate; dy <= dilate && ! opaque; ++dy)
                for (int dx = -dilate; dx <= dilate && ! opaque; ++dx)
                {
                    const int sx = x + dx, sy = y + dy;
                    if (sx >= 0 && sy >= 0 && sx < source.width && sy < source.height
                        && ((juce::PixelARGB*) source.getPixelPointer(sx, sy))->getAlpha() >= threshold)
                        opaque = true;
                }

            auto* pixel = (juce::PixelARGB*) destination.getPixelPointer(x, y);
            const juce::uint8 value = opaque ? 255 : 0;
            pixel->setARGB(value, value, value, value);
        }
    return result;
}

void paintPanelBackground(juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius)
{
    juce::Graphics::ScopedSaveState savedState(g);
    auto b = bounds.reduced(0.5f);
    g.setColour(juce::Colours::black.withAlpha(0.48f));
    g.fillRoundedRectangle(b.translated(0.0f, 2.0f), cornerRadius);

    juce::ColourGradient surface(Colours::panel2, b.getX(), b.getY(), Colours::panel, b.getX(), b.getBottom(), false);
    surface.addColour(0.12, juce::Colour(0xff15191f));
    g.setGradientFill(surface);
    g.fillRoundedRectangle(b, cornerRadius);

    g.saveState();
    juce::Path clip;
    clip.addRoundedRectangle(b, cornerRadius);
    g.reduceClipRegion(clip);
    paintMetalTexture(g, b, 0.075f);
    g.restoreState();

    g.setColour(Colours::line.withAlpha(0.85f));
    g.drawRoundedRectangle(b, cornerRadius, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.045f));
    g.drawRoundedRectangle(b.reduced(1.0f), juce::jmax(1.0f, cornerRadius - 1.0f), 1.0f);
    g.setColour(Colours::blue.withAlpha(0.11f));
    g.drawHorizontalLine(juce::roundToInt(b.getY() + 1.0f), b.getX() + cornerRadius, b.getRight() - cornerRadius);
}

void makeDbLabelEditable(juce::Label& label, juce::Slider& slider)
{
    label.setEditable(false, true, false);
    label.onTextChange = [&label, &slider]
    {
        const auto numeric = label.getText().retainCharacters("-0123456789.");
        if (numeric.isNotEmpty())
        {
            const double value = juce::jlimit(slider.getMinimum(), slider.getMaximum(), numeric.getDoubleValue());
            slider.setValue(value, juce::sendNotificationSync);
        }
        label.setText(juce::String(slider.getValue(), 1) + " dB", juce::dontSendNotification);
    };
}

DrumVaultLookAndFeel::DrumVaultLookAndFeel()
{
    setDefaultSansSerifTypeface(SwarmFonts::uiLight(13.0f).getTypefacePtr());

    setColour(juce::ResizableWindow::backgroundColourId, Colours::bg);
    setColour(juce::TextButton::buttonColourId, Colours::panel2);
    setColour(juce::TextButton::buttonOnColourId, Colours::blueDark);
    setColour(juce::TextButton::textColourOffId, Colours::text);
    setColour(juce::TextButton::textColourOnId, Colours::blueBright);
    setColour(juce::Label::textColourId, Colours::text);

    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff07090c));
    setColour(juce::ComboBox::textColourId, Colours::text);
    setColour(juce::ComboBox::outlineColourId, Colours::blue.withAlpha(0.48f));
    setColour(juce::ComboBox::arrowColourId, Colours::blue);

    setColour(juce::Slider::backgroundColourId, Colours::bg);
    setColour(juce::Slider::trackColourId, Colours::blue);
    setColour(juce::Slider::thumbColourId, Colours::blueBright);
    setColour(juce::Slider::textBoxTextColourId, Colours::blueBright);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff050609));
    setColour(juce::Slider::textBoxOutlineColourId, Colours::line);

    setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff05070a));
    setColour(juce::TextEditor::textColourId, Colours::text);
    setColour(juce::TextEditor::outlineColourId, Colours::line);
    setColour(juce::TextEditor::focusedOutlineColourId, Colours::blue);
    setColour(juce::TextEditor::highlightColourId, Colours::blue.withAlpha(0.28f));

    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff0b0d12));
    setColour(juce::PopupMenu::textColourId, Colours::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Colours::blue.withAlpha(0.24f));
    setColour(juce::PopupMenu::highlightedTextColourId, Colours::blueBright);
    setColour(juce::ScrollBar::thumbColourId, Colours::blue.withAlpha(0.42f));
    setColour(juce::ScrollBar::trackColourId, Colours::bg.withAlpha(0.8f));

    buttonOffImage = loadImage(BinaryData::mothbuttonoff_png, BinaryData::mothbuttonoff_pngSize);
    buttonOnImage = loadImage(BinaryData::mothbuttonon_png, BinaryData::mothbuttonon_pngSize);
    sliderThumbImage = loadImage(BinaryData::mothsliderthumb_png, BinaryData::mothsliderthumb_pngSize);
}

juce::Font DrumVaultLookAndFeel::displayFont(float height, bool)
{
    return SwarmFonts::title(height);
}

juce::Font DrumVaultLookAndFeel::bodyFont(float height, bool bold)
{
    return bold ? SwarmFonts::uiMedium(height) : SwarmFonts::uiLight(height);
}

juce::Font DrumVaultLookAndFeel::numericFont(float height)
{
    return SwarmFonts::numeric(height);
}

juce::Font DrumVaultLookAndFeel::getLabelFont(juce::Label& label)
{
    return label.getFont();
}

juce::Font DrumVaultLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return SwarmFonts::uiMedium(juce::jlimit(10.0f, 15.0f, (float) buttonHeight * 0.34f));
}

juce::Font DrumVaultLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return SwarmFonts::uiLight(juce::jlimit(10.0f, 14.0f, (float) box.getHeight() * 0.46f));
}

juce::Font DrumVaultLookAndFeel::getPopupMenuFont()
{
    return SwarmFonts::uiLight(15.0f);
}

void DrumVaultLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                 bool highlighted, bool down)
{
    if ((bool) button.getProperties().getWithDefault("brand", false))
        return;

    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool primary = button.getProperties().getWithDefault("primary", false);
    const bool danger = button.getProperties().getWithDefault("danger", false);
    const bool toggled = button.getToggleState();
    const auto configuredOn = button.findColour(juce::TextButton::buttonOnColourId);
    const auto accent = danger ? Colours::red
                      : (toggled && configuredOn != Colours::blueDark ? configuredOn : Colours::blue);
    const float radius = juce::jlimit(3.0f, 7.0f, bounds.getHeight() * 0.18f);

    if (toggled || primary)
    {
        juce::Path glowPath;
        glowPath.addRoundedRectangle(bounds, radius);
        juce::DropShadow(accent.withAlpha(toggled ? 0.33f : 0.22f),
                         juce::roundToInt(bounds.getHeight() * 0.34f), {}).drawForPath(g, glowPath);
    }

    const auto& chrome = toggled || primary ? buttonOnImage : buttonOffImage;
    const auto source = toggled || primary ? buttonOnInterior : buttonOffInterior;
    if (chrome.isValid())
        drawFlatChrome(g, chrome, source, bounds, radius);
    else
    {
        juce::ColourGradient fallback(Colours::panel2.brighter(0.08f), bounds.getTopLeft(),
                                      juce::Colour(0xff06070a), bounds.getBottomRight(), false);
        g.setGradientFill(fallback);
        g.fillRoundedRectangle(bounds, radius);
    }

    if (toggled || primary)
    {
        g.setColour(accent.withAlpha(0.11f));
        g.fillRoundedRectangle(bounds, radius);
    }
    if (highlighted || down)
    {
        g.setColour(juce::Colours::white.withAlpha(down ? 0.055f : 0.085f));
        g.fillRoundedRectangle(bounds, radius);
    }

    g.setColour(accent.withAlpha(button.isEnabled() ? (toggled || primary ? 0.92f : 0.46f) : 0.22f));
    g.drawRoundedRectangle(bounds, radius, toggled || primary ? 1.4f : 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.045f));
    g.drawHorizontalLine(juce::roundToInt(bounds.getY() + 1.0f), bounds.getX() + radius, bounds.getRight() - radius);
}

void DrumVaultLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool highlighted, bool)
{
    if ((bool) button.getProperties().getWithDefault("brand", false))
        return;

    const bool primary = button.getProperties().getWithDefault("primary", false);
    const bool danger = button.getProperties().getWithDefault("danger", false);
    const bool toggled = button.getToggleState();
    const auto configuredOn = button.findColour(juce::TextButton::buttonOnColourId);
    const auto accent = danger ? Colours::red
                      : (toggled && configuredOn != Colours::blueDark ? configuredOn : Colours::blueBright);
    auto colour = toggled || primary || highlighted ? accent : Colours::text;
    g.setColour(colour.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.42f));
    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.drawFittedText(primary ? withLetterSpacing(button.getButtonText()) : button.getButtonText(),
                     button.getLocalBounds().reduced(4, 0), juce::Justification::centred, 1, 0.82f);
}

void DrumVaultLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float, float,
                                             const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height);
    const bool vertical = style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical;
    const bool enabled = slider.isEnabled();

    if (vertical)
    {
        const float trackWidth = 6.0f;
        const juce::Rectangle<float> track(bounds.getCentreX() - trackWidth * 0.5f, bounds.getY(), trackWidth, bounds.getHeight());
        g.setColour(juce::Colour(0xff030406));
        g.fillRoundedRectangle(track, trackWidth * 0.5f);
        g.setColour(Colours::lineSoft);
        g.drawRoundedRectangle(track, trackWidth * 0.5f, 1.0f);

        auto fill = track.withTop(juce::jlimit(track.getY(), track.getBottom(), sliderPos));
        g.setColour(Colours::blue.withAlpha(enabled ? 0.88f : 0.35f));
        g.fillRoundedRectangle(fill, trackWidth * 0.5f);

        const float diameter = juce::jmin(19.0f, bounds.getWidth() * 0.75f);
        auto thumb = juce::Rectangle<float>(diameter, diameter).withCentre({ bounds.getCentreX(), sliderPos });
        if (sliderThumbImage.isValid())
            g.drawImage(sliderThumbImage, thumb);
        else
        {
            g.setColour(Colours::blueBright);
            g.fillEllipse(thumb);
        }
        g.setColour(Colours::blue.withAlpha(0.72f));
        g.drawEllipse(thumb, 1.0f);
    }
    else
    {
        const float trackHeight = 6.0f;
        const juce::Rectangle<float> track(bounds.getX(), bounds.getCentreY() - trackHeight * 0.5f,
                                            bounds.getWidth(), trackHeight);
        g.setColour(juce::Colour(0xff030406));
        g.fillRoundedRectangle(track, trackHeight * 0.5f);
        g.setColour(Colours::lineSoft);
        g.drawRoundedRectangle(track, trackHeight * 0.5f, 1.0f);

        auto fill = track.withRight(juce::jlimit(track.getX(), track.getRight(), sliderPos));
        g.setColour(Colours::blue.withAlpha(enabled ? 0.88f : 0.35f));
        g.fillRoundedRectangle(fill, trackHeight * 0.5f);

        const float diameter = juce::jmin(17.0f, bounds.getHeight() * 0.78f);
        auto thumb = juce::Rectangle<float>(diameter, diameter).withCentre({ sliderPos, bounds.getCentreY() });
        if (sliderThumbImage.isValid())
            g.drawImage(sliderThumbImage, thumb);
        else
        {
            g.setColour(Colours::blueBright);
            g.fillEllipse(thumb);
        }
        g.setColour(Colours::blue.withAlpha(0.72f));
        g.drawEllipse(thumb, 1.0f);
    }
}

void DrumVaultLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                         int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height).reduced(1.0f);
    const float radius = juce::jlimit(3.0f, 6.0f, bounds.getHeight() * 0.2f);
    g.setColour(juce::Colour(0xff050609).withAlpha(box.isEnabled() ? 0.96f : 0.58f));
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(Colours::blue.withAlpha(box.isPopupActive() || isButtonDown ? 0.92f : 0.48f));
    g.drawRoundedRectangle(bounds, radius, box.isPopupActive() ? 1.4f : 1.0f);

    auto arrowZone = bounds.removeFromRight((float) height * 0.9f);
    const float size = (float) height * 0.23f;
    const auto centre = arrowZone.getCentre();
    juce::Path chevron;
    chevron.startNewSubPath(centre.x - size * 0.6f, centre.y - size * 0.25f);
    chevron.lineTo(centre.x, centre.y + size * 0.38f);
    chevron.lineTo(centre.x + size * 0.6f, centre.y - size * 0.25f);
    g.setColour(Colours::blue.withAlpha(box.isEnabled() ? 0.92f : 0.35f));
    g.strokePath(chevron, juce::PathStrokeType(juce::jmax(1.2f, size * 0.18f),
                                               juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

} // namespace dsl::ui
