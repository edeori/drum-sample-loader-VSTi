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

        triggerNoteOn(*kitSnapshot, message.getNoteNumber(), (int) message.getVelocity());
    }

    voicePool.renderNextBlock(busBuffers, numSamples);

    const int masterBusIndex = kitSnapshot->busRouting.indexOfBus("bus:Master");
    if (masterBusIndex >= 0 && masterBusIndex < (int) busBuffers.size() && busBuffers[(size_t) masterBusIndex] != nullptr)
        busBuffers[(size_t) masterBusIndex]->applyGain(juce::Decibels::decibelsToGain(kitSnapshot->masterDb));
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

void DrumEngine::triggerNoteOn(Kit& kitRef, int midiNote, int velocity)
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
        if (micDef != nullptr)
        {
            pan = micDef->pan;
            // Overhead/Room mics capture the whole kit, so their output
            // destination — and their Global Mixer mute/solo state — is one
            // kit-wide choice, not per instrument.
            if (isOverheadRole(micDef->role))
            {
                gainLinear *= juce::Decibels::decibelsToGain(kitRef.masterOverheadDb);
                busId = kitRef.overheadBusId;
                channelMuted = kitRef.overheadMuted;
                channelSoloed = kitRef.overheadSolo;
            }
            else if (isRoomRole(micDef->role))
            {
                gainLinear *= juce::Decibels::decibelsToGain(kitRef.masterRoomDb);
                busId = kitRef.roomBusId;
                channelMuted = kitRef.roomMuted;
                channelSoloed = kitRef.roomSolo;
            }
        }

        if (channelMuted || (anySoloedChannel && ! channelSoloed))
            continue;

        int busIndex = busId.isNotEmpty() ? kitRef.busRouting.indexOfBus(busId) : -1;
        if (busIndex < 0)
            busIndex = fallbackMasterBus;

        voicePool.startVoice(sampleRef.buffer, sampleRef.sourceSampleRate, gainLinear, busIndex,
                             pitchRatio, pan, voiceGroup);
    }
}

} // namespace dsl
