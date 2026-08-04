#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <map>
#include <memory>
#include <vector>

namespace dsl
{

/** A single preloaded sample, shared (read-only) between the pool that owns it
    and any voices currently playing it. Loaded once per unique file path by
    SamplePool, never mutated after load, so sharing across voices is safe
    without locking.
*/
struct SampleRef
{
    juce::String sourcePath;                                   // preset-relative or absolute path, kept for (de)serialization
    std::shared_ptr<const juce::AudioBuffer<float>> buffer;     // null until SamplePool has loaded it
    double sourceSampleRate = 44100.0;

    bool isLoaded() const noexcept { return buffer != nullptr; }
};

/** One round-robin slot = every mic-channel recording captured from a single,
    real, simultaneous hit (docs/03-editor-ui.md, section 3.3). A channel with
    no recording for this particular hit is simply absent from the map.
*/
struct RoundRobinSlot
{
    std::map<juce::String, SampleRef> channelSamples; // micChannel id -> sample

    const SampleRef* find(const juce::String& micChannelId) const
    {
        auto it = channelSamples.find(micChannelId);
        return it == channelSamples.end() ? nullptr : &it->second;
    }
};

/** One velocity layer's round-robin pool for a single instrument. The layer's
    display name lives in the kit-wide GlobalVelocityConfig::layerNames (indexed
    by position) rather than here, so there is a single source of truth for it.
*/
struct VelocityLayer
{
    std::vector<RoundRobinSlot> slots;

    // Runtime-only state (not part of the preset, see docs/06-preset-format.md):
    // the last slot index played, so the round-robin picker can exclude it.
    int lastPlayedSlot = -1;
};

} // namespace dsl
