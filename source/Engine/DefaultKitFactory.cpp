#include "DefaultKitFactory.h"
#include "SampleImportMatcher.h"
#include <algorithm>

namespace dsl
{

namespace
{
    juce::String normalizedInstrumentName(juce::String value)
    {
        return value.toLowerCase().removeCharacters(" _-");
    }

    void addAmbienceChannels(Instrument& inst, MicRole role, AmbienceFormat format)
    {
        // No channelRouting entries here: Overhead/Room routing is a single
        // kit-wide choice (Kit::overheadBusId/roomBusId), not per instrument
        // — see DrumEngine::triggerNoteOn and docs/04-mixing-routing.md.
        const bool overhead = role == MicRole::Overhead;
        const juce::String baseId = overhead ? "OH" : "Room";
        const juce::String displayName = overhead ? "Overhead" : "Room";

        if (format == AmbienceFormat::Stereo)
        {
            inst.micChannels.push_back({ baseId, displayName + " (Stereo)", role, 2, 0.0f });
            inst.channelFaderDb[baseId] = 0.0f;
            return;
        }

        const auto leftId = baseId + "_L";
        const auto rightId = baseId + "_R";
        inst.micChannels.push_back({ leftId, displayName + " L", role, 1, -1.0f });
        inst.micChannels.push_back({ rightId, displayName + " R", role, 1, 1.0f });
        inst.channelFaderDb[leftId] = 0.0f;
        inst.channelFaderDb[rightId] = 0.0f;
    }

    void addSharedAmbienceChannels(Instrument& inst, AmbienceFormat overheadFormat, AmbienceFormat roomFormat)
    {
        addAmbienceChannels(inst, MicRole::Overhead, overheadFormat);
        addAmbienceChannels(inst, MicRole::Room, roomFormat);
    }

    void addArticulation(Instrument& inst, juce::String id, juce::String name, int midiNote,
                         ArticulationAction action = ArticulationAction::Trigger)
    {
        InstrumentArticulation articulation;
        articulation.id = std::move(id);
        articulation.name = std::move(name);
        articulation.midiNote = midiNote;
        articulation.action = action;
        inst.articulations.push_back(std::move(articulation));
    }

    Instrument makeBodyInstrument(juce::String id, juce::String name, int midiNote,
                                   const std::vector<std::pair<juce::String, juce::String>>& spotChannelIdAndBus,
                                   AmbienceFormat overheadFormat, AmbienceFormat roomFormat)
    {
        Instrument inst;
        inst.id = std::move(id);
        inst.name = std::move(name);
        inst.category = InstrumentCategory::Body;
        addArticulation(inst, "main", "Main", midiNote);

        for (auto& [spotId, spotBus] : spotChannelIdAndBus)
        {
            inst.micChannels.push_back({ spotId, spotId + " (Spot)", MicRole::Spot, 1, 0.0f });
            inst.channelRouting[spotId] = spotBus;
            inst.channelFaderDb[spotId] = 0.0f;
        }

        addSharedAmbienceChannels(inst, overheadFormat, roomFormat);
        return inst;
    }

    Instrument makeCymbalInstrument(juce::String id, juce::String name, int midiNote,
                                    AmbienceFormat overheadFormat, AmbienceFormat roomFormat)
    {
        Instrument inst;
        inst.id = std::move(id);
        inst.name = std::move(name);
        inst.category = InstrumentCategory::Cymbal;
        addArticulation(inst, "main", "Main", midiNote);

        // Every instrument exposes a Spot channel. It may intentionally stay
        // empty when that recording session did not use a dedicated mic.
        inst.micChannels.push_back({ "Spot", "Spot", MicRole::Spot, 1, 0.0f });
        inst.channelRouting["Spot"] = "bus:Master";
        inst.channelFaderDb["Spot"] = 0.0f;

        addSharedAmbienceChannels(inst, overheadFormat, roomFormat);
        return inst;
    }

    int availableNote(Kit& kit, int preferred)
    {
        return kit.targetForMidiNote(preferred).articulation == nullptr ? preferred : -1;
    }

    InstrumentArticulation& addRequiredArticulation(Kit& kit, Instrument& instrument,
                                                     juce::String id, juce::String name,
                                                     int preferredNote,
                                                     ArticulationAction action = ArticulationAction::Trigger)
    {
        InstrumentArticulation articulation;
        articulation.id = std::move(id);
        articulation.name = std::move(name);
        articulation.midiNote = availableNote(kit, preferredNote);
        articulation.action = action;
        articulation.velocityLayers.resize((size_t) kit.velocityConfig.getLayerCount());
        instrument.articulations.push_back(std::move(articulation));
        return instrument.articulations.back();
    }

    bool slotLooksLikeChoke(const RoundRobinSlot& slot)
    {
        for (auto& [channelId, sample] : slot.channelSamples)
            if (SampleImportMatcher::hasChokeMarker(sample.sourcePath))
                return true;
        return false;
    }

    bool slotLooksLikeBell(const RoundRobinSlot& slot)
    {
        for (auto& [channelId, sample] : slot.channelSamples)
            if (sample.sourcePath.containsIgnoreCase("bell"))
                return true;
        return false;
    }

    bool articulationContainsSampleMarker(const InstrumentArticulation& articulation,
                                           const juce::String& marker)
    {
        for (auto& layer : articulation.velocityLayers)
            for (auto& slot : layer.slots)
                for (auto& [channelId, sample] : slot.channelSamples)
                    if (sample.sourcePath.containsIgnoreCase(marker))
                        return true;
        return false;
    }

    int articulationOrder(const InstrumentArticulation& articulation)
    {
        if (articulation.id == "closed" || articulation.id == "hit") return 0;
        if (articulation.id == "half_open" || articulation.id == "choke"
            || articulation.id == "bell") return 1;
        if (articulation.id == "open") return 2;
        return 3;
    }
}

Kit DefaultKitFactory::createDefaultKit()
{
    Kit kit;
    kit.kitName = "Default Kit";

    // Default routing uses the generic output pool (docs/04-mixing-routing.md,
    // section 4.2) — these map 1:1 onto real host outputs once loaded in a
    // DAW as a multi-out instrument, exactly like Kontakt's numbered outs.
    constexpr auto mono = AmbienceFormat::MonoPair;
    kit.overheadBusId = "bus:Out9";
    kit.roomBusId = "bus:Out10";

    // Note numbers are the placeholder "Krimh-style" template documented in
    // docs/05-midi-mapping.md section 5.2 — GM + common metal-production
    // convention, not a verified official Krimh Drums export.
    kit.instruments.push_back(makeBodyInstrument("kick", "Kick", 36,
        { { "In", "bus:Out1" }, { "Out", "bus:Out2" } }, mono, mono));

    kit.instruments.push_back(makeBodyInstrument("snare", "Snare", 38,
        { { "Top", "bus:Out3" }, { "Bottom", "bus:Out4" } }, mono, mono));

    kit.instruments.push_back(makeBodyInstrument("tom1", "Tom 1", 50, { { "Spot", "bus:Out5" } }, mono, mono));
    kit.instruments.push_back(makeBodyInstrument("tom2", "Tom 2", 48, { { "Spot", "bus:Out6" } }, mono, mono));
    kit.instruments.push_back(makeBodyInstrument("tom3", "Tom 3", 45, { { "Spot", "bus:Out7" } }, mono, mono));
    kit.instruments.push_back(makeBodyInstrument("tom4", "Tom 4", 41, { { "Spot", "bus:Out8" } }, mono, mono));

    // Hi-hat articulations are independent MIDI triggers and keep independent
    // velocity layers/round-robin sample pools.
    auto hihat = makeCymbalInstrument("hihat", "Hi-hat", 42, mono, mono);
    hihat.articulations.front().id = "closed";
    hihat.articulations.front().name = "Closed";
    addArticulation(hihat, "half_open", "Half Open", 44);
    addArticulation(hihat, "open", "Open", 46);
    kit.instruments.push_back(std::move(hihat));

    auto crash1 = makeCymbalInstrument("crash1", "Crash 1", 49, mono, mono);
    crash1.articulations.front().id = "hit";
    crash1.articulations.front().name = "Hit";
    addArticulation(crash1, "choke", "Choke", 61, ArticulationAction::Choke);
    kit.instruments.push_back(std::move(crash1));

    auto crash2 = makeCymbalInstrument("crash2", "Crash 2", 57, mono, mono);
    crash2.articulations.front().id = "hit";
    crash2.articulations.front().name = "Hit";
    addArticulation(crash2, "choke", "Choke", 58, ArticulationAction::Choke);
    kit.instruments.push_back(std::move(crash2));

    auto ride = makeCymbalInstrument("ride", "Ride", 51, mono, mono);
    ride.articulations.front().id = "hit";
    ride.articulations.front().name = "Hit";
    addArticulation(ride, "bell", "Bell", 53);
    kit.instruments.push_back(std::move(ride));

    auto china = makeCymbalInstrument("china", "China", 52, mono, mono);
    china.articulations.front().id = "hit";
    china.articulations.front().name = "Hit";
    addArticulation(china, "choke", "Choke", 60, ArticulationAction::Choke);
    kit.instruments.push_back(std::move(china));
    kit.instruments.push_back(makeCymbalInstrument("splash", "Splash", 55, mono, mono));

    // Ensures every articulation's velocityLayers array matches the global
    // layer count/thresholds, all starting empty (no samples assigned yet).
    kit.setVelocityLayerCount(kit.velocityConfig.getLayerCount());

    return kit;
}

void DefaultKitFactory::ensureRequiredArticulations(Kit& kit)
{
    for (auto& instrument : kit.instruments)
    {
        const auto identity = normalizedInstrumentName(instrument.id + instrument.name);
        const bool isHihat = identity.contains("hihat");
        const bool isCrash = instrument.category == InstrumentCategory::Cymbal
                          && identity.contains("crash");
        const bool isChina = instrument.category == InstrumentCategory::Cymbal
                          && identity.contains("china");
        const bool isRide = instrument.category == InstrumentCategory::Cymbal
                         && identity.contains("ride");

        if (isHihat)
        {
            auto* closed = instrument.findArticulation("closed");
            if (closed == nullptr)
            {
                if (auto* main = instrument.findArticulation("main"))
                {
                    // Old single-articulation presets may already contain a
                    // specifically named Open/Half sample set. Infer that
                    // articulation from the source paths rather than blindly
                    // relabelling real Open samples as Closed.
                    if (articulationContainsSampleMarker(*main, "(open)"))
                    {
                        main->id = "open";
                        main->name = "Open";
                        if (availableNote(kit, 46) >= 0)
                            main->midiNote = 46;
                    }
                    else if (articulationContainsSampleMarker(*main, "(half)"))
                    {
                        main->id = "half_open";
                        main->name = "Half Open";
                        if (availableNote(kit, 44) >= 0)
                            main->midiNote = 44;
                    }
                    else
                    {
                        main->id = "closed";
                        main->name = "Closed";
                    }
                    closed = instrument.findArticulation("closed");
                }
                else
                {
                    closed = &addRequiredArticulation(kit, instrument, "closed", "Closed", 42);
                }
            }

            if (closed == nullptr)
                closed = &addRequiredArticulation(kit, instrument, "closed", "Closed", 42);
            closed->name = "Closed";
            closed->action = ArticulationAction::Trigger;

            if (instrument.findArticulation("half_open") == nullptr)
                addRequiredArticulation(kit, instrument, "half_open", "Half Open", 44);
            if (instrument.findArticulation("open") == nullptr)
                addRequiredArticulation(kit, instrument, "open", "Open", 46);

            std::stable_sort(instrument.articulations.begin(), instrument.articulations.end(),
                             [](const auto& a, const auto& b)
                             { return articulationOrder(a) < articulationOrder(b); });
        }

        if (isCrash || isChina)
        {
            auto* hit = instrument.findArticulation("hit");
            if (hit == nullptr)
            {
                if (auto* main = instrument.findArticulation("main"))
                {
                    main->id = "hit";
                    main->name = "Hit";
                    hit = main;
                }
                else
                {
                    const int preferredHitNote = isChina ? 52
                        : (identity.contains("crash2") ? 57 : 49);
                    hit = &addRequiredArticulation(kit, instrument, "hit", "Hit", preferredHitNote);
                }
            }
            hit->name = "Hit";
            hit->action = ArticulationAction::Trigger;
            hit->velocityLayers.resize((size_t) kit.velocityConfig.getLayerCount());

            auto* choke = instrument.findArticulation("choke");
            if (choke == nullptr)
            {
                const int preferredChokeNote = isChina ? 60
                    : (identity.contains("crash2") ? 58 : 61);
                choke = &addRequiredArticulation(kit, instrument, "choke", "Choke",
                                                 preferredChokeNote, ArticulationAction::Choke);
            }
            // Adding Choke may reallocate the articulation vector.
            hit = instrument.findArticulation("hit");
            choke = instrument.findArticulation("choke");
            choke->name = "Choke";
            choke->action = ArticulationAction::Choke;
            choke->velocityLayers.resize((size_t) kit.velocityConfig.getLayerCount());

            // Older builds could import explicitly named choke samples into
            // the only available Main grid. Move those complete multi-mic
            // slots into the new Choke articulation during the upgrade.
            const int layerCount = juce::jmin((int) hit->velocityLayers.size(),
                                              (int) choke->velocityLayers.size());
            for (int layerIndex = 0; layerIndex < layerCount; ++layerIndex)
            {
                auto& hitSlots = hit->velocityLayers[(size_t) layerIndex].slots;
                auto& chokeSlots = choke->velocityLayers[(size_t) layerIndex].slots;
                for (auto it = hitSlots.begin(); it != hitSlots.end();)
                {
                    if (slotLooksLikeChoke(*it))
                    {
                        chokeSlots.push_back(std::move(*it));
                        it = hitSlots.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            std::stable_sort(instrument.articulations.begin(), instrument.articulations.end(),
                             [](const auto& a, const auto& b)
                             { return articulationOrder(a) < articulationOrder(b); });
        }

        if (isRide)
        {
            auto* hit = instrument.findArticulation("hit");
            if (hit == nullptr)
            {
                if (auto* main = instrument.findArticulation("main"))
                {
                    main->id = "hit";
                    main->name = "Hit";
                    hit = main;
                }
                else
                {
                    hit = &addRequiredArticulation(kit, instrument, "hit", "Hit", 51);
                }
            }
            hit->name = "Hit";
            hit->action = ArticulationAction::Trigger;
            hit->velocityLayers.resize((size_t) kit.velocityConfig.getLayerCount());

            auto* bell = instrument.findArticulation("bell");
            if (bell == nullptr)
                bell = &addRequiredArticulation(kit, instrument, "bell", "Bell", 53);

            // Adding Bell may reallocate the articulation vector.
            hit = instrument.findArticulation("hit");
            bell = instrument.findArticulation("bell");
            bell->name = "Bell";
            bell->action = ArticulationAction::Trigger;
            bell->velocityLayers.resize((size_t) kit.velocityConfig.getLayerCount());

            // Preserve already imported, explicitly named bell samples by
            // moving their complete multi-mic slots out of the legacy Main grid.
            const int layerCount = juce::jmin((int) hit->velocityLayers.size(),
                                              (int) bell->velocityLayers.size());
            for (int layerIndex = 0; layerIndex < layerCount; ++layerIndex)
            {
                auto& hitSlots = hit->velocityLayers[(size_t) layerIndex].slots;
                auto& bellSlots = bell->velocityLayers[(size_t) layerIndex].slots;
                for (auto it = hitSlots.begin(); it != hitSlots.end();)
                {
                    if (slotLooksLikeBell(*it))
                    {
                        bellSlots.push_back(std::move(*it));
                        it = hitSlots.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            std::stable_sort(instrument.articulations.begin(), instrument.articulations.end(),
                             [](const auto& a, const auto& b)
                             { return articulationOrder(a) < articulationOrder(b); });
        }
    }
}

Instrument DefaultKitFactory::createBlankInstrument(juce::String id, juce::String name,
                                                      InstrumentCategory category, int midiNote,
                                                      AmbienceFormat overheadFormat,
                                                      AmbienceFormat roomFormat)
{
    if (category == InstrumentCategory::Cymbal)
        return makeCymbalInstrument(std::move(id), std::move(name), midiNote, overheadFormat, roomFormat);

    return makeBodyInstrument(std::move(id), std::move(name), midiNote,
                              { { "Spot", "bus:Master" } }, overheadFormat, roomFormat);
}

bool DefaultKitFactory::reconfigureAmbienceChannels(Instrument& instrument, MicRole role, AmbienceFormat format)
{
    if (! isAmbienceRole(role))
        return false;

    const juce::String baseId = role == MicRole::Overhead ? "OH" : "Room";
    const auto isStandardChannel = [&baseId, role](const MicChannelDef& channel)
    {
        return channel.role == role
            && (channel.id == baseId || channel.id == baseId + "_L" || channel.id == baseId + "_R");
    };

    std::vector<juce::String> oldIds;
    size_t insertionIndex = instrument.micChannels.size();
    bool currentlyStereo = false;
    float preservedFaderDb = 0.0f;

    for (size_t i = 0; i < instrument.micChannels.size(); ++i)
    {
        auto& channel = instrument.micChannels[i];
        if (! isStandardChannel(channel))
            continue;

        insertionIndex = juce::jmin(insertionIndex, i);
        oldIds.push_back(channel.id);
        currentlyStereo = currentlyStereo || channel.numChannels == 2;
        if (oldIds.size() == 1)
            preservedFaderDb = instrument.getChannelFaderDb(channel.id);
    }

    const bool wantsStereo = format == AmbienceFormat::Stereo;
    if (! oldIds.empty() && currentlyStereo == wantsStereo
        && ((wantsStereo && oldIds.size() == 1) || (! wantsStereo && oldIds.size() == 2)))
        return true;

    instrument.micChannels.erase(
        std::remove_if(instrument.micChannels.begin(), instrument.micChannels.end(),
                       isStandardChannel),
        instrument.micChannels.end());

    for (auto& id : oldIds)
    {
        instrument.channelFaderDb.erase(id);
        instrument.channelRouting.erase(id);
        instrument.channelMuted.erase(id);
        instrument.channelSolo.erase(id);
    }

    Instrument replacement;
    addAmbienceChannels(replacement, role, format);
    for (auto& [id, db] : replacement.channelFaderDb)
        instrument.channelFaderDb[id] = preservedFaderDb;

    insertionIndex = juce::jmin(insertionIndex, instrument.micChannels.size());
    instrument.micChannels.insert(instrument.micChannels.begin() + (std::ptrdiff_t) insertionIndex,
                                  replacement.micChannels.begin(), replacement.micChannels.end());
    return true;
}

} // namespace dsl
