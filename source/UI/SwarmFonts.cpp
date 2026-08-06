#include "SwarmFonts.h"
#include <BinaryData.h>

namespace dsl::ui::SwarmFonts
{
namespace
{
juce::Typeface::Ptr loadTypeface(const void* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor(data, (size_t) size);
}

juce::Font fromTypeface(const juce::Typeface::Ptr& typeface, float height)
{
    return juce::Font(juce::FontOptions(typeface)).withHeight(height);
}
}

juce::Font title(float height)
{
    static auto typeface = loadTypeface(BinaryData::OxaniumBold_ttf, BinaryData::OxaniumBold_ttfSize);
    return fromTypeface(typeface, height);
}

juce::Font uiLight(float height)
{
    static auto typeface = loadTypeface(BinaryData::BarlowCondensedLight_ttf, BinaryData::BarlowCondensedLight_ttfSize);
    return fromTypeface(typeface, height);
}

juce::Font uiMedium(float height)
{
    static auto typeface = loadTypeface(BinaryData::BarlowCondensedMedium_ttf, BinaryData::BarlowCondensedMedium_ttfSize);
    return fromTypeface(typeface, height);
}

juce::Font numeric(float height)
{
    static auto typeface = loadTypeface(BinaryData::RajdhaniMedium_ttf, BinaryData::RajdhaniMedium_ttfSize);
    return fromTypeface(typeface, height);
}

} // namespace dsl::ui::SwarmFonts
