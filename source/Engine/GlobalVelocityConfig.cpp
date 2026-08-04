#include "GlobalVelocityConfig.h"
#include <algorithm>

namespace dsl
{

GlobalVelocityConfig::GlobalVelocityConfig()
{
    // Default 4-layer split, matches docs/03-editor-ui.md section 3.2.
    layerNames = { "Ghost", "Soft", "Medium", "Heavy" };
    thresholds = { 32, 64, 96 };
}

void GlobalVelocityConfig::setLayerCount(int newCount)
{
    newCount = juce::jmax(1, newCount);
    if (newCount == getLayerCount())
        return;

    layerNames.resize((size_t) newCount);
    thresholds.resize((size_t) juce::jmax(0, newCount - 1));

    renumberDefaultNames();

    // Any newly-grown threshold slot starts at 0 (default-constructed int);
    // spread those evenly across the velocity range instead of leaving them at 0.
    for (size_t i = 0; i < thresholds.size(); ++i)
    {
        if (thresholds[i] == 0)
            thresholds[i] = (int) juce::jmap((double) (i + 1), 0.0, (double) thresholds.size() + 1.0, 1.0, 126.0);
    }

    // Defensive: keep strictly ascending after any resize/regeneration.
    std::sort(thresholds.begin(), thresholds.end());
    for (size_t i = 1; i < thresholds.size(); ++i)
        if (thresholds[i] <= thresholds[i - 1])
            thresholds[i] = juce::jmin(126, thresholds[i - 1] + 1);
}

int GlobalVelocityConfig::selectLayerIndex(int velocity) const noexcept
{
    velocity = juce::jlimit(0, 127, velocity);
    int layer = 0;
    for (int threshold : thresholds)
    {
        if (velocity < threshold)
            break;
        ++layer;
    }
    return juce::jmin(layer, getLayerCount() - 1);
}

int GlobalVelocityConfig::previewVelocityForLayer(int layerIndex) const noexcept
{
    if (getLayerCount() <= 0)
        return 100;

    layerIndex = juce::jlimit(0, getLayerCount() - 1, layerIndex);
    const int lower = layerIndex > 0 ? thresholds[(size_t) layerIndex - 1] : 0;
    const int upper = layerIndex < (int) thresholds.size()
        ? thresholds[(size_t) layerIndex] - 1 : 127;
    return juce::jlimit(1, 127, (lower + upper) / 2);
}

void GlobalVelocityConfig::renumberDefaultNames()
{
    static const char* defaults[] = { "Ghost", "Soft", "Medium", "Heavy" };

    for (size_t i = 0; i < layerNames.size(); ++i)
        if (layerNames[i].isEmpty())
            layerNames[i] = i < 4 ? juce::String(defaults[i]) : ("Layer " + juce::String((int) i + 1));
}

} // namespace dsl
