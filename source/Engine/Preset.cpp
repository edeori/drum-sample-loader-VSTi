#include "Preset.h"
#include "DefaultKitFactory.h"

namespace dsl
{

namespace
{
    juce::String categoryToString(InstrumentCategory c)
    {
        return c == InstrumentCategory::Body ? "Body" : "Cymbal";
    }

    InstrumentCategory categoryFromString(const juce::String& s)
    {
        return s == "Cymbal" ? InstrumentCategory::Cymbal : InstrumentCategory::Body;
    }

    juce::String roleToString(MicRole r)
    {
        switch (r)
        {
            case MicRole::Spot:     return "Spot";
            case MicRole::Overhead: return "Overhead";
            case MicRole::Room:     return "Room";
        }
        return "Spot";
    }

    MicRole roleFromString(const juce::String& s)
    {
        if (s == "Overhead") return MicRole::Overhead;
        if (s == "Room")     return MicRole::Room;
        return MicRole::Spot;
    }

    juce::String actionToString(ArticulationAction action)
    {
        return action == ArticulationAction::Choke ? "Choke" : "Trigger";
    }

    ArticulationAction actionFromString(const juce::String& value)
    {
        return value == "Choke" ? ArticulationAction::Choke : ArticulationAction::Trigger;
    }

    juce::Array<juce::var> layersToVar(const std::vector<VelocityLayer>& layers)
    {
        juce::Array<juce::var> layersArr;
        for (int li = 0; li < (int) layers.size(); ++li)
        {
            auto layerObj = std::make_unique<juce::DynamicObject>();
            layerObj->setProperty("layerIndex", li);

            juce::Array<juce::var> slotsArr;
            for (auto& slot : layers[(size_t) li].slots)
            {
                auto slotObj = std::make_unique<juce::DynamicObject>();
                for (auto& [chId, sampleRef] : slot.channelSamples)
                    slotObj->setProperty(chId, sampleRef.sourcePath);
                slotsArr.add(juce::var(slotObj.release()));
            }
            layerObj->setProperty("roundRobinSlots", slotsArr);
            layersArr.add(juce::var(layerObj.release()));
        }
        return layersArr;
    }

    void layersFromVar(const juce::var& value, std::vector<VelocityLayer>& layers)
    {
        auto* layersArr = value.getArray();
        if (layersArr == nullptr)
            return;

        layers.resize((size_t) layersArr->size());
        for (auto& layerVar : *layersArr)
        {
            const int li = (int) layerVar.getProperty("layerIndex", 0);
            if (li < 0 || li >= (int) layers.size())
                continue;

            auto& layer = layers[(size_t) li];
            if (auto* slotsArr = layerVar.getProperty("roundRobinSlots", {}).getArray())
            {
                for (auto& slotVar : *slotsArr)
                {
                    RoundRobinSlot slot;
                    if (auto* slotObj = slotVar.getDynamicObject())
                        for (auto& prop : slotObj->getProperties())
                        {
                            SampleRef ref;
                            ref.sourcePath = prop.value.toString();
                            slot.channelSamples[prop.name.toString()] = ref;
                        }

                    layer.slots.push_back(std::move(slot));
                }
            }
        }
    }
}

juce::var Preset::toVar(const Kit& kit)
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("presetVersion", 2);
    root->setProperty("kitName", kit.kitName);

    {
        auto velocityObj = std::make_unique<juce::DynamicObject>();
        juce::Array<juce::var> names;
        for (auto& n : kit.velocityConfig.layerNames)
            names.add(n);
        juce::Array<juce::var> thresholds;
        for (auto t : kit.velocityConfig.thresholds)
            thresholds.add(t);
        velocityObj->setProperty("layerNames", names);
        velocityObj->setProperty("thresholds", thresholds);
        root->setProperty("globalVelocityLayers", juce::var(velocityObj.release()));
    }

    {
        auto masterObj = std::make_unique<juce::DynamicObject>();
        masterObj->setProperty("master_dB", (double) kit.masterDb);
        masterObj->setProperty("masterOverhead_dB", (double) kit.masterOverheadDb);
        masterObj->setProperty("masterRoom_dB", (double) kit.masterRoomDb);
        masterObj->setProperty("masterTune_st", (double) kit.masterTuneSemitones);
        root->setProperty("masterFaders", juce::var(masterObj.release()));
    }

    {
        auto routingObj = std::make_unique<juce::DynamicObject>();
        routingObj->setProperty("overheadBusId", kit.overheadBusId);
        routingObj->setProperty("roomBusId", kit.roomBusId);
        routingObj->setProperty("overheadMuted", kit.overheadMuted);
        routingObj->setProperty("overheadSolo", kit.overheadSolo);
        routingObj->setProperty("roomMuted", kit.roomMuted);
        routingObj->setProperty("roomSolo", kit.roomSolo);
        root->setProperty("globalRouting", juce::var(routingObj.release()));
    }

    juce::Array<juce::var> instrumentsArr;
    for (auto& inst : kit.instruments)
    {
        auto instObj = std::make_unique<juce::DynamicObject>();
        instObj->setProperty("id", inst.id);
        instObj->setProperty("name", inst.name);
        instObj->setProperty("category", categoryToString(inst.category));
        instObj->setProperty("muted", inst.muted);
        instObj->setProperty("solo", inst.solo);

        juce::Array<juce::var> articulationsArr;
        for (auto& articulation : inst.articulations)
        {
            auto articulationObj = std::make_unique<juce::DynamicObject>();
            articulationObj->setProperty("id", articulation.id);
            articulationObj->setProperty("name", articulation.name);
            articulationObj->setProperty("midiNote", articulation.midiNote);
            articulationObj->setProperty("action", actionToString(articulation.action));
            articulationObj->setProperty("velocityLayers", layersToVar(articulation.velocityLayers));
            articulationsArr.add(juce::var(articulationObj.release()));
        }
        instObj->setProperty("articulations", articulationsArr);

        juce::Array<juce::var> micChannelsArr;
        for (auto& ch : inst.micChannels)
        {
            auto chObj = std::make_unique<juce::DynamicObject>();
            chObj->setProperty("id", ch.id);
            chObj->setProperty("displayName", ch.displayName);
            chObj->setProperty("role", roleToString(ch.role));
            chObj->setProperty("numChannels", ch.numChannels);
            chObj->setProperty("pan", (double) ch.pan);
            micChannelsArr.add(juce::var(chObj.release()));
        }
        instObj->setProperty("micChannels", micChannelsArr);

        auto fadersObj = std::make_unique<juce::DynamicObject>();
        for (auto& [chId, db] : inst.channelFaderDb)
            fadersObj->setProperty(chId, (double) db);
        instObj->setProperty("channelFaders_dB", juce::var(fadersObj.release()));

        auto routingObj = std::make_unique<juce::DynamicObject>();
        for (auto& [chId, busId] : inst.channelRouting)
            routingObj->setProperty(chId, busId);
        instObj->setProperty("channelRouting", juce::var(routingObj.release()));

        auto channelMutedObj = std::make_unique<juce::DynamicObject>();
        for (auto& [chId, m] : inst.channelMuted)
            channelMutedObj->setProperty(chId, m);
        instObj->setProperty("channelMuted", juce::var(channelMutedObj.release()));

        auto channelSoloObj = std::make_unique<juce::DynamicObject>();
        for (auto& [chId, s] : inst.channelSolo)
            channelSoloObj->setProperty(chId, s);
        instObj->setProperty("channelSolo", juce::var(channelSoloObj.release()));

        instrumentsArr.add(juce::var(instObj.release()));
    }
    root->setProperty("instruments", instrumentsArr);

    return juce::var(root.release());
}

juce::String Preset::toJsonString(const Kit& kit, bool pretty)
{
    return juce::JSON::toString(toVar(kit), ! pretty);
}

Kit Preset::fromVar(const juce::var& json)
{
    Kit kit;
    kit.kitName = json.getProperty("kitName", "Untitled Kit").toString();

    if (auto* velocityObj = json.getProperty("globalVelocityLayers", {}).getDynamicObject())
    {
        kit.velocityConfig.layerNames.clear();
        if (auto* namesArr = velocityObj->getProperty("layerNames").getArray())
            for (auto& n : *namesArr)
                kit.velocityConfig.layerNames.push_back(n.toString());

        kit.velocityConfig.thresholds.clear();
        if (auto* thrArr = velocityObj->getProperty("thresholds").getArray())
            for (auto& t : *thrArr)
                kit.velocityConfig.thresholds.push_back((int) t);
    }

    if (auto* masterObj = json.getProperty("masterFaders", {}).getDynamicObject())
    {
        kit.masterDb = (float) (double) masterObj->getProperty("master_dB");
        kit.masterOverheadDb = (float) (double) masterObj->getProperty("masterOverhead_dB");
        kit.masterRoomDb = (float) (double) masterObj->getProperty("masterRoom_dB");
        kit.masterTuneSemitones = (float) (double) masterObj->getProperty("masterTune_st");
    }

    if (auto* routingObj = json.getProperty("globalRouting", {}).getDynamicObject())
    {
        kit.overheadBusId = routingObj->getProperty("overheadBusId").toString();
        kit.roomBusId = routingObj->getProperty("roomBusId").toString();
        kit.overheadMuted = (bool) routingObj->getProperty("overheadMuted");
        kit.overheadSolo = (bool) routingObj->getProperty("overheadSolo");
        kit.roomMuted = (bool) routingObj->getProperty("roomMuted");
        kit.roomSolo = (bool) routingObj->getProperty("roomSolo");
    }

    if (auto* instrumentsArr = json.getProperty("instruments", {}).getArray())
    {
        for (auto& instVar : *instrumentsArr)
        {
            Instrument inst;
            inst.id = instVar.getProperty("id", {}).toString();
            inst.name = instVar.getProperty("name", {}).toString();
            inst.category = categoryFromString(instVar.getProperty("category", "Body").toString());
            inst.muted = (bool) instVar.getProperty("muted", false);
            inst.solo = (bool) instVar.getProperty("solo", false);

            if (auto* articulationsArr = instVar.getProperty("articulations", {}).getArray())
            {
                for (auto& articulationVar : *articulationsArr)
                {
                    InstrumentArticulation articulation;
                    articulation.id = articulationVar.getProperty("id", "main").toString();
                    articulation.name = articulationVar.getProperty("name", "Main").toString();
                    articulation.midiNote = (int) articulationVar.getProperty("midiNote", -1);
                    articulation.action = actionFromString(articulationVar.getProperty("action", "Trigger").toString());
                    layersFromVar(articulationVar.getProperty("velocityLayers", {}), articulation.velocityLayers);
                    inst.articulations.push_back(std::move(articulation));
                }
            }

            // Backward compatibility with presetVersion 1: its single
            // midiNote/velocityLayers pair becomes one Main articulation.
            if (inst.articulations.empty())
            {
                InstrumentArticulation main;
                main.id = "main";
                main.name = "Main";
                main.midiNote = (int) instVar.getProperty("midiNote", -1);
                layersFromVar(instVar.getProperty("velocityLayers", {}), main.velocityLayers);
                inst.articulations.push_back(std::move(main));
            }

            if (auto* micArr = instVar.getProperty("micChannels", {}).getArray())
            {
                for (auto& chVar : *micArr)
                {
                    MicChannelDef ch;
                    ch.id = chVar.getProperty("id", {}).toString();
                    ch.displayName = chVar.getProperty("displayName", {}).toString();
                    ch.role = roleFromString(chVar.getProperty("role", "Spot").toString());
                    ch.numChannels = (int) chVar.getProperty("numChannels", 1);
                    const double legacyDefaultPan = ch.id.endsWith("_L") ? -1.0 : ch.id.endsWith("_R") ? 1.0 : 0.0;
                    ch.pan = (float) (double) chVar.getProperty("pan", legacyDefaultPan);
                    inst.micChannels.push_back(ch);
                }
            }

            bool hasSpotChannel = false;
            for (auto& channel : inst.micChannels)
                hasSpotChannel = hasSpotChannel || channel.role == MicRole::Spot;
            const bool addedSpotChannel = ! hasSpotChannel;
            if (addedSpotChannel)
                inst.micChannels.insert(inst.micChannels.begin(), { "Spot", "Spot", MicRole::Spot, 1, 0.0f });

            if (auto* fadersObj = instVar.getProperty("channelFaders_dB", {}).getDynamicObject())
                for (auto& prop : fadersObj->getProperties())
                    inst.channelFaderDb[prop.name.toString()] = (float) (double) prop.value;

            if (auto* routingObj = instVar.getProperty("channelRouting", {}).getDynamicObject())
                for (auto& prop : routingObj->getProperties())
                    inst.channelRouting[prop.name.toString()] = prop.value.toString();

            if (auto* channelMutedObj = instVar.getProperty("channelMuted", {}).getDynamicObject())
                for (auto& prop : channelMutedObj->getProperties())
                    inst.channelMuted[prop.name.toString()] = (bool) prop.value;

            if (auto* channelSoloObj = instVar.getProperty("channelSolo", {}).getDynamicObject())
                for (auto& prop : channelSoloObj->getProperties())
                    inst.channelSolo[prop.name.toString()] = (bool) prop.value;

            if (addedSpotChannel && inst.channelFaderDb.find("Spot") == inst.channelFaderDb.end())
                inst.channelFaderDb["Spot"] = 0.0f;
            if (addedSpotChannel && inst.channelRouting.find("Spot") == inst.channelRouting.end())
                inst.channelRouting["Spot"] = "bus:Master";

            kit.instruments.push_back(std::move(inst));
        }
    }

    DefaultKitFactory::ensureRequiredArticulations(kit);
    return kit;
}

Kit Preset::fromJsonString(const juce::String& jsonText)
{
    return fromVar(juce::JSON::parse(jsonText));
}

} // namespace dsl
