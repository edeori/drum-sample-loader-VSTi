#include "Kit.h"

namespace dsl
{

Instrument* Kit::findInstrumentById(const juce::String& instrumentId)
{
    for (auto& instrument : instruments)
        if (instrument.id == instrumentId)
            return &instrument;

    return nullptr;
}

const Instrument* Kit::findInstrumentById(const juce::String& instrumentId) const
{
    for (auto& instrument : instruments)
        if (instrument.id == instrumentId)
            return &instrument;

    return nullptr;
}

Instrument* Kit::instrumentForMidiNote(int midiNote)
{
    return targetForMidiNote(midiNote).instrument;
}

Kit::MidiTarget Kit::targetForMidiNote(int midiNote)
{
    for (auto& instrument : instruments)
        for (auto& articulation : instrument.articulations)
            if (articulation.midiNote == midiNote)
                return { &instrument, &articulation };

    return {};
}

Kit::SetNoteResult Kit::setInstrumentMidiNote(const juce::String& instrumentId, int midiNote, bool force)
{
    auto* target = findInstrumentById(instrumentId);
    if (target == nullptr || target->primaryArticulation() == nullptr)
        return SetNoteResult::Conflict;

    return setArticulationMidiNote(instrumentId, target->primaryArticulation()->id, midiNote, force);
}

Kit::SetNoteResult Kit::setArticulationMidiNote(const juce::String& instrumentId,
                                                 const juce::String& articulationId,
                                                 int midiNote, bool force)
{
    auto* targetInstrument = findInstrumentById(instrumentId);
    auto* target = targetInstrument != nullptr ? targetInstrument->findArticulation(articulationId) : nullptr;
    if (target == nullptr)
        return SetNoteResult::Conflict;

    // -1 is the explicit "unassigned" value. It cannot conflict with another
    // unassigned articulation and is useful when a forced reassignment clears
    // the previous owner.
    if (midiNote < 0)
    {
        target->midiNote = -1;
        return SetNoteResult::Ok;
    }

    if (midiNote > 127)
        return SetNoteResult::Conflict;

    auto owner = targetForMidiNote(midiNote);
    if (owner.articulation != nullptr && owner.articulation != target)
    {
        if (! force)
            return SetNoteResult::Conflict;

        owner.articulation->midiNote = -1;
    }

    target->midiNote = midiNote;
    return SetNoteResult::Ok;
}

void Kit::setVelocityLayerCount(int newCount)
{
    velocityConfig.setLayerCount(newCount);

    for (auto& instrument : instruments)
        for (auto& articulation : instrument.articulations)
            resizeArticulationVelocityLayers(articulation, velocityConfig.getLayerCount());
}

void Kit::resizeArticulationVelocityLayers(InstrumentArticulation& articulation, int newCount)
{
    auto& layers = articulation.velocityLayers;

    // Shrinking: move the slots of any layer about to be dropped into
    // orphanedSlots so the user's recorded samples are never silently lost.
    for (size_t i = (size_t) newCount; i < layers.size(); ++i)
        for (auto& slot : layers[i].slots)
            articulation.orphanedSlots.push_back(std::move(slot));

    layers.resize((size_t) newCount);
}

} // namespace dsl
