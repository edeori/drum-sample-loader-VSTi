#include "DrumEngine.h"
#include "RoundRobinSelector.h"
#include <cmath>

namespace dsl
{

DrumEngine::DrumEngine(int maxVoices)
    : voicePool(maxVoices)
{
}

void DrumEngine::prepare(double sampleRateToUse, int /*maxBlockSize*/)
{
    sampleRate = sampleRateToUse;
    voicePool.prepare(sampleRate);
}

void DrumEngine::setKit(std::shared_ptr<Kit> newKit)
{
    std::lock_guard<std::mutex> lock(kitMutex);
    kit = std::move(newKit);
}

std::shared_ptr<Kit> DrumEngine::getKit() const
{
    std::lock_guard<std::mutex> lock(kitMutex);
    return kit;
}

void DrumEngine::processBlock(const juce::MidiBuffer& midiMessages,
                               const std::vector<juce::AudioBuffer<float>*>& busBuffers,
                               int numSamples)
{
    std::shared_ptr<Kit> kitSnapshot;
    {
        std::lock_guard<std::mutex> lock(kitMutex);
        kitSnapshot = kit;
    }

    if (kitSnapshot == nullptr)
        return;

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        if (! message.isNoteOn())
            continue;

        if (midiLearnActive.load(std::memory_order_acquire))
        {
            std::lock_guard<std::mutex> lock(midiLearnMutex);
            midiLearnResult = MidiLearnResult { midiLearnInstrumentId, midiLearnArticulationId, message.getNoteNumber() };
            midiLearnActive.store(false, std::memory_order_release);
            continue; // learn-mode note-ons are captured, not played
        }

        triggerNoteOn(*kitSnapshot, message.getNoteNumber(), (int) message.getVelocity(), busBuffers);
    }

    voicePool.renderNextBlock(busBuffers, numSamples);

    // Master is a true final trim over everything, not just whatever's
    // routed to bus:Master — every populated bus gets it, on top of its own
    // per-channel/per-bus routing (docs/04-mixing-routing.md section 4.4).
    const float masterGain = juce::Decibels::decibelsToGain(kitSnapshot->masterDb);
    for (auto* buf : busBuffers)
        if (buf != nullptr && buf->getNumChannels() > 0)
            buf->applyGain(masterGain);
}

void DrumEngine::beginMidiLearn(const juce::String& instrumentId, const juce::String& articulationId)
{
    std::lock_guard<std::mutex> lock(midiLearnMutex);
    midiLearnInstrumentId = instrumentId;
    midiLearnArticulationId = articulationId;
    midiLearnResult.reset();
    midiLearnActive.store(true, std::memory_order_release);
}

void DrumEngine::cancelMidiLearn()
{
    midiLearnActive.store(false, std::memory_order_release);
}

std::optional<DrumEngine::MidiLearnResult> DrumEngine::consumeMidiLearnResult()
{
    std::lock_guard<std::mutex> lock(midiLearnMutex);
    auto result = midiLearnResult;
    midiLearnResult.reset();
    return result;
}

uint64_t DrumEngine::meterKeyFor(const juce::String& instrumentId, const MicChannelDef& micDef)
{
    if (isOverheadRole(micDef.role))
        return (uint64_t) juce::String("__pooled_OH__").hashCode64();
    if (isRoomRole(micDef.role))
        return (uint64_t) juce::String("__pooled_Room__").hashCode64();

    return (uint64_t) (instrumentId + ":" + micDef.id).hashCode64();
}

void DrumEngine::triggerNoteOn(Kit& kitRef, int midiNote, int velocity,
                               const std::vector<juce::AudioBuffer<float>*>& busBuffers)
{
    auto target = kitRef.targetForMidiNote(midiNote);
    auto* instrument = target.instrument;
    auto* articulation = target.articulation;
    if (instrument == nullptr || articulation == nullptr)
        return;

    if (midiNote >= 0 && midiNote < 128)
        lastHitTimeMs[(size_t) midiNote].store(juce::Time::getMillisecondCounter(), std::memory_order_relaxed);

    const auto voiceGroup = (uint64_t) instrument->id.hashCode64();
    if (articulation->action == ArticulationAction::Choke)
        voicePool.chokeVoices(voiceGroup);

    // A choke is a performance command, so it must still stop the ringing
    // instrument if that instrument was muted or excluded by another solo
    // after the original hit started.
    if (instrument->muted)
        return;

    if (kitRef.hasAnySoloedInstrument() && ! instrument->solo)
        return;

    const int requestedLayerIndex = kitRef.velocityConfig.selectLayerIndex(velocity);
    if (requestedLayerIndex < 0 || requestedLayerIndex >= (int) articulation->velocityLayers.size())
        return;

    // If the requested layer has no samples yet, prefer the next HARDER layer
    // that does (never a softer one) — an unfinished kit should still sound
    // like a real hit rather than silently drop in intensity. Only fall back
    // downward as a last resort, so something still plays if only soft
    // layers have been authored so far.
    int layerIndex = -1;
    for (int i = requestedLayerIndex; i < (int) articulation->velocityLayers.size(); ++i)
    {
        if (! articulation->velocityLayers[(size_t) i].slots.empty())
        {
            layerIndex = i;
            break;
        }
    }
    if (layerIndex < 0)
    {
        for (int i = requestedLayerIndex - 1; i >= 0; --i)
        {
            if (! articulation->velocityLayers[(size_t) i].slots.empty())
            {
                layerIndex = i;
                break;
            }
        }
    }
    if (layerIndex < 0)
        return;

    auto& layer = articulation->velocityLayers[(size_t) layerIndex];
    const int slotIndex = selectRoundRobinSlot((int) layer.slots.size(), layer.lastPlayedSlot, rng);
    if (slotIndex < 0)
        return;

    layer.lastPlayedSlot = slotIndex;
    auto& slot = layer.slots[(size_t) slotIndex];

    const int fallbackMasterBus = kitRef.busRouting.indexOfBus("bus:Master");
    const double pitchRatio = std::pow(2.0, (double) kitRef.masterTuneSemitones / 12.0);
    const bool anySoloedChannel = kitRef.hasAnySoloedChannel();

    for (auto& [channelId, sampleRef] : slot.channelSamples)
    {
        if (! sampleRef.isLoaded())
            continue;

        // Channel-layout switches retain the inactive topology's sample
        // mappings so switching back is lossless. Only mappings belonging to
        // the currently visible mic-channel layout may render.
        auto* micDef = instrument->findChannel(channelId);
        if (micDef == nullptr)
            continue;

        float gainLinear = juce::Decibels::decibelsToGain(instrument->getChannelFaderDb(channelId));
        juce::String busId = instrument->getChannelRouting(channelId);
        bool channelMuted = instrument->isChannelMuted(channelId);
        bool channelSoloed = instrument->isChannelSoloed(channelId);

        float pan = 0.0f;
        bool swapStereo = false;
        if (micDef != nullptr)
        {
            pan = micDef->pan;
            // Overhead/Room mics capture the whole kit, so their output
            // destination — and their Global Mixer mute/solo/swap state —
            // is one kit-wide choice, not per instrument.
            if (isOverheadRole(micDef->role))
            {
                gainLinear *= juce::Decibels::decibelsToGain(kitRef.masterOverheadDb);
                busId = kitRef.overheadBusId;
                channelMuted = kitRef.overheadMuted;
                channelSoloed = kitRef.overheadSolo;
                if (kitRef.overheadSwapLR)
                {
                    if (micDef->numChannels == 1)
                        pan = -pan;
                    else
                        swapStereo = true;
                }
            }
            else if (isRoomRole(micDef->role))
            {
                gainLinear *= juce::Decibels::decibelsToGain(kitRef.masterRoomDb);
                busId = kitRef.roomBusId;
                channelMuted = kitRef.roomMuted;
                channelSoloed = kitRef.roomSolo;
                if (kitRef.roomSwapLR)
                {
                    if (micDef->numChannels == 1)
                        pan = -pan;
                    else
                        swapStereo = true;
                }
            }
            else if (instrument->isChannelSwapped(channelId))
            {
                // Spot channels are always declared mono in the kit (no
                // per-instrument "this Spot is stereo" setting exists), but
                // nothing stops a genuinely stereo file from being loaded
                // into one — so the swap decision is made from what's
                // actually loaded for this hit, not the channel's static
                // declaration.
                if (sampleRef.buffer->getNumChannels() == 1)
                    pan = -pan;
                else
                    swapStereo = true;
            }
        }

        if (channelMuted || (anySoloedChannel && ! channelSoloed))
            continue;

        // Falls back to Master both when the routing table doesn't know the
        // bus id, and when it does but the host/standalone this session
        // doesn't actually expose that many real output channels (e.g. a
        // plain 2-channel Standalone device only ever gives bus 0 real
        // channels) — docs/04-mixing-routing.md section 4.2's documented
        // "falls back to the internal stereo mix" behavior.
        int busIndex = busId.isNotEmpty() ? kitRef.busRouting.indexOfBus(busId) : -1;
        const bool resolvedBusUsable = busIndex >= 0 && busIndex < (int) busBuffers.size()
            && busBuffers[(size_t) busIndex] != nullptr && busBuffers[(size_t) busIndex]->getNumChannels() > 0;
        if (! resolvedBusUsable)
            busIndex = fallbackMasterBus;

        voicePool.startVoice(sampleRef.buffer, sampleRef.sourceSampleRate, gainLinear, busIndex,
                             pitchRatio, pan, voiceGroup, meterKeyFor(instrument->id, *micDef), swapStereo);
    }
}

} // namespace dsl
