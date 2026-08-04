#include "KitEditController.h"
#include "../Engine/DefaultKitFactory.h"
#include "../Engine/DrumsetExporter.h"
#include "../Engine/Preset.h"
#include "../Engine/SampleImportMatcher.h"
#include <algorithm>

namespace dsl::ui
{

KitEditController::KitEditController(DrumSampleLoaderAudioProcessor& processorToUse)
    : processor(processorToUse)
{
    if (auto kit = processor.getEngine().getKit())
        workingKit = *kit;

    DefaultKitFactory::ensureRequiredArticulations(workingKit);
    pushToEngine();

    if (! workingKit.instruments.empty())
        selectedInstrumentId = workingKit.instruments.front().id;

    startTimerHz(30);
}

KitEditController::~KitEditController()
{
    stopTimer();
}

Instrument* KitEditController::getSelectedInstrument()
{
    return workingKit.findInstrumentById(selectedInstrumentId);
}

InstrumentArticulation* KitEditController::getSelectedArticulation()
{
    auto* instrument = getSelectedInstrument();
    if (instrument == nullptr || selectedArticulationIndex < 0
        || selectedArticulationIndex >= (int) instrument->articulations.size())
        return nullptr;

    return &instrument->articulations[(size_t) selectedArticulationIndex];
}

int KitEditController::getOrphanedSampleCount() const
{
    int count = 0;
    for (auto& inst : workingKit.instruments)
        for (auto& articulation : inst.articulations)
            for (auto& slot : articulation.orphanedSlots)
                count += (int) slot.channelSamples.size();
    return count;
}

void KitEditController::selectInstrument(const juce::String& instrumentId)
{
    if (workingKit.findInstrumentById(instrumentId) == nullptr)
        return;

    selectedInstrumentId = instrumentId;
    selectedArticulationIndex = 0;
    notifyListeners();
}

void KitEditController::selectArticulation(int articulationIndex)
{
    auto* instrument = getSelectedInstrument();
    if (instrument == nullptr || instrument->articulations.empty())
        return;

    selectedArticulationIndex = juce::jlimit(0, (int) instrument->articulations.size() - 1, articulationIndex);
    notifyListeners();
}

void KitEditController::selectLayer(int layerIndex)
{
    selectedLayerIndex = juce::jlimit(0, juce::jmax(0, workingKit.velocityConfig.getLayerCount() - 1), layerIndex);
    notifyListeners();
}

void KitEditController::addInstrument(const juce::String& name, InstrumentCategory category, int midiNote,
                                       AmbienceFormat overheadFormat, AmbienceFormat roomFormat)
{
    auto id = makeUniqueId(name);
    auto inst = DefaultKitFactory::createBlankInstrument(id, name, category, midiNote, overheadFormat, roomFormat);

    if (workingKit.instrumentForMidiNote(midiNote) != nullptr)
        inst.primaryArticulation()->midiNote = -1; // requested note already taken — leave unassigned

    for (auto& articulation : inst.articulations)
        articulation.velocityLayers.resize((size_t) workingKit.velocityConfig.getLayerCount());

    workingKit.instruments.push_back(std::move(inst));
    DefaultKitFactory::ensureRequiredArticulations(workingKit);
    selectedInstrumentId = id;
    selectedArticulationIndex = 0;
    selectedLayerIndex = 0;

    pushToEngine();
    notifyListeners();
}

void KitEditController::removeInstrument(const juce::String& instrumentId)
{
    if (workingKit.instruments.size() <= 1)
        return;

    auto it = std::find_if(workingKit.instruments.begin(), workingKit.instruments.end(),
                            [&](const Instrument& i) { return i.id == instrumentId; });
    if (it == workingKit.instruments.end())
        return;

    const bool wasSelected = instrumentId == selectedInstrumentId;
    workingKit.instruments.erase(it);

    if (wasSelected && ! workingKit.instruments.empty())
    {
        selectedInstrumentId = workingKit.instruments.front().id;
        selectedArticulationIndex = 0;
    }

    pushToEngine();
    notifyListeners();
}

void KitEditController::renameInstrument(const juce::String& instrumentId, const juce::String& newName)
{
    const auto trimmed = newName.trim();
    if (trimmed.isEmpty())
        return;

    if (auto* inst = workingKit.findInstrumentById(instrumentId))
    {
        inst->name = trimmed;
        pushToEngine();
        notifyListeners();
    }
}

void KitEditController::toggleMute(const juce::String& instrumentId)
{
    if (auto* inst = workingKit.findInstrumentById(instrumentId))
    {
        inst->muted = ! inst->muted;
        pushToEngine();
        notifyListeners();
    }
}

void KitEditController::toggleSolo(const juce::String& instrumentId)
{
    if (auto* inst = workingKit.findInstrumentById(instrumentId))
    {
        inst->solo = ! inst->solo;
        pushToEngine();
        notifyListeners();
    }
}

Kit::SetNoteResult KitEditController::setInstrumentMidiNote(const juce::String& instrumentId, int midiNote, bool force)
{
    auto result = workingKit.setInstrumentMidiNote(instrumentId, midiNote, force);
    if (result == Kit::SetNoteResult::Ok)
    {
        pushToEngine();
        notifyListeners();
    }
    return result;
}

Kit::SetNoteResult KitEditController::setArticulationMidiNote(const juce::String& instrumentId,
                                                               const juce::String& articulationId,
                                                               int midiNote, bool force)
{
    auto result = workingKit.setArticulationMidiNote(instrumentId, articulationId, midiNote, force);
    if (result == Kit::SetNoteResult::Ok)
    {
        pushToEngine();
        notifyListeners();
    }
    return result;
}

void KitEditController::beginMidiLearn(const juce::String& instrumentId, const juce::String& articulationId)
{
    midiLearnArmedInstrumentId = instrumentId;
    midiLearnArmedArticulationId = articulationId;
    processor.getEngine().beginMidiLearn(instrumentId, articulationId);
    notifyListeners();
}

void KitEditController::cancelMidiLearn()
{
    midiLearnArmedInstrumentId.clear();
    midiLearnArmedArticulationId.clear();
    processor.getEngine().cancelMidiLearn();
    notifyListeners();
}

void KitEditController::addRoundRobinSlot()
{
    auto* articulation = getSelectedArticulation();
    if (articulation == nullptr || selectedLayerIndex < 0 || selectedLayerIndex >= (int) articulation->velocityLayers.size())
        return;

    articulation->velocityLayers[(size_t) selectedLayerIndex].slots.push_back({});
    pushToEngine();
    notifyListeners();
}

void KitEditController::removeRoundRobinSlot(int slotIndex)
{
    auto* articulation = getSelectedArticulation();
    if (articulation == nullptr || selectedLayerIndex < 0 || selectedLayerIndex >= (int) articulation->velocityLayers.size())
        return;

    auto& slots = articulation->velocityLayers[(size_t) selectedLayerIndex].slots;
    if (slotIndex < 0 || slotIndex >= (int) slots.size())
        return;

    slots.erase(slots.begin() + slotIndex);
    pushToEngine();
    notifyListeners();
}

void KitEditController::assignSampleToCell(const juce::String& micChannelId, int slotIndex)
{
    activeChooser = std::make_shared<juce::FileChooser>("Select a drum sample", juce::File(), "*.wav;*.aiff;*.aif");
    const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    activeChooser->launchAsync(flags, [this, micChannelId, slotIndex](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        activeChooser.reset();
        if (! file.existsAsFile())
            return;

        double sourceSampleRate = 44100.0;
        auto buffer = processor.getSamplePool().loadOrGet(file.getFullPathName(), sourceSampleRate);
        if (buffer == nullptr)
            return;

        auto* articulation = getSelectedArticulation();
        if (articulation == nullptr || selectedLayerIndex < 0
            || selectedLayerIndex >= (int) articulation->velocityLayers.size())
            return;

        auto& slots = articulation->velocityLayers[(size_t) selectedLayerIndex].slots;
        if (slotIndex < 0 || slotIndex >= (int) slots.size())
            return;

        SampleRef ref;
        ref.sourcePath = file.getFullPathName();
        ref.buffer = buffer;
        ref.sourceSampleRate = sourceSampleRate;
        slots[(size_t) slotIndex].channelSamples[micChannelId] = ref;

        pushToEngine();
        notifyListeners();
    });
}

void KitEditController::clearSampleCell(const juce::String& micChannelId, int slotIndex)
{
    auto* articulation = getSelectedArticulation();
    if (articulation == nullptr || selectedLayerIndex < 0
        || selectedLayerIndex >= (int) articulation->velocityLayers.size())
        return;

    auto& slots = articulation->velocityLayers[(size_t) selectedLayerIndex].slots;
    if (slotIndex < 0 || slotIndex >= (int) slots.size())
        return;

    slots[(size_t) slotIndex].channelSamples.erase(micChannelId);
    pushToEngine();
    notifyListeners();
}

bool KitEditController::setSelectedAmbienceFormats(AmbienceFormat overheadFormat, AmbienceFormat roomFormat)
{
    auto* instrument = getSelectedInstrument();
    if (instrument == nullptr)
        return false;

    // Validate both changes on a copy first so applying two format changes is
    // atomic if one side still contains assigned samples.
    auto updated = *instrument;
    if (! DefaultKitFactory::reconfigureAmbienceChannels(updated, MicRole::Overhead, overheadFormat)
        || ! DefaultKitFactory::reconfigureAmbienceChannels(updated, MicRole::Room, roomFormat))
        return false;

    *instrument = std::move(updated);
    pushToEngine();
    notifyListeners();
    return true;
}

bool KitEditController::commitImportedSample(const juce::String& instrumentId,
                                              const juce::String& articulationId,
                                              int layerIndex, const juce::String& channelId,
                                              const juce::File& file,
                                              std::shared_ptr<const juce::AudioBuffer<float>> buffer,
                                              double sourceSampleRate)
{
    auto* instrument = workingKit.findInstrumentById(instrumentId);
    auto* articulation = instrument != nullptr ? instrument->findArticulation(articulationId) : nullptr;
    if (instrument == nullptr || articulation == nullptr || instrument->findChannel(channelId) == nullptr
        || layerIndex < 0 || layerIndex >= (int) articulation->velocityLayers.size() || buffer == nullptr)
        return false;

    SampleRef ref;
    ref.sourcePath = file.getFullPathName();
    ref.buffer = std::move(buffer);
    ref.sourceSampleRate = sourceSampleRate;

    RoundRobinSlot slot;
    slot.channelSamples[channelId] = std::move(ref);
    articulation->velocityLayers[(size_t) layerIndex].slots.push_back(std::move(slot));
    pushToEngine();
    notifyListeners();
    return true;
}

bool KitEditController::commitImportedStereoSample(const juce::String& instrumentId,
                                                    const juce::String& articulationId,
                                                    int layerIndex, MicRole destinationRole,
                                                    const juce::File& file,
                                                    std::shared_ptr<const juce::AudioBuffer<float>> buffer,
                                                    double sourceSampleRate)
{
    auto* instrument = workingKit.findInstrumentById(instrumentId);
    if (instrument == nullptr || ! isAmbienceRole(destinationRole))
        return false;

    juce::String stereoChannelId;
    for (auto& channel : instrument->micChannels)
        if (channel.role == destinationRole && channel.numChannels == 2)
            stereoChannelId = channel.id;

    if (stereoChannelId.isEmpty())
    {
        auto updated = *instrument;
        if (! DefaultKitFactory::reconfigureAmbienceChannels(
                updated, destinationRole, AmbienceFormat::Stereo))
            return false;

        *instrument = std::move(updated);
        for (auto& channel : instrument->micChannels)
            if (channel.role == destinationRole && channel.numChannels == 2)
                stereoChannelId = channel.id;
    }

    return stereoChannelId.isNotEmpty()
        && commitImportedSample(instrumentId, articulationId, layerIndex, stereoChannelId,
                                file, std::move(buffer), sourceSampleRate);
}

void KitEditController::importSamplesToSelectedInstrument(bool forceStereoImport,
                                                           std::function<void(SampleImportResult)> onComplete)
{
    auto* inst = getSelectedInstrument();
    auto* articulation = getSelectedArticulation();
    if (inst == nullptr || articulation == nullptr || selectedLayerIndex < 0
        || selectedLayerIndex >= (int) articulation->velocityLayers.size())
        return;

    activeChooser = std::make_shared<juce::FileChooser>("Select drum samples to import", juce::File(), "*.wav;*.aiff;*.aif");
    const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                        | juce::FileBrowserComponent::canSelectMultipleItems;

    const juce::String instrumentId = inst->id;
    const juce::String articulationId = articulation->id;
    const int layerIndex = selectedLayerIndex;

    activeChooser->launchAsync(flags, [this, instrumentId, articulationId, layerIndex,
                                       forceStereoImport, onComplete](const juce::FileChooser& fc)
    {
        auto files = fc.getResults();
        activeChooser.reset();
        if (files.isEmpty())
            return;

        auto* targetInst = workingKit.findInstrumentById(instrumentId);
        auto* targetArticulation = targetInst != nullptr ? targetInst->findArticulation(articulationId) : nullptr;
        if (targetInst == nullptr || targetArticulation == nullptr || layerIndex < 0
            || layerIndex >= (int) targetArticulation->velocityLayers.size())
            return;

        // Some multitrack packs provide an additional mono centre Room mic
        // alongside the normal Room L/R pair. Materialise that channel only
        // when such files are actually imported, keeping the default layout
        // compact for packs that do not have it.
        bool needsRoomMidChannel = false;
        for (auto& file : files)
            needsRoomMidChannel = needsRoomMidChannel
                || SampleImportMatcher::hasRoomMidMarker(file.getFileNameWithoutExtension());

        if (needsRoomMidChannel && targetInst->findChannel("Room_M") == nullptr)
        {
            auto insertionPoint = targetInst->micChannels.end();
            for (auto it = targetInst->micChannels.begin(); it != targetInst->micChannels.end(); ++it)
                if (it->role == MicRole::Room)
                    insertionPoint = std::next(it);

            targetInst->micChannels.insert(insertionPoint,
                { "Room_M", "Room M", MicRole::Room, 1, 0.0f });
            targetInst->channelFaderDb["Room_M"] = 0.0f;
        }

        // A single interleaved-stereo file is a distinct, common workflow.
        // Inspect the actual audio instead of requiring its filename to carry
        // an OH/Room suffix. If both stereo destinations exist and the name is
        // ambiguous, ask explicitly rather than guessing or dropping it.
        if (files.size() == 1)
        {
            const auto file = files.getReference(0);
            double sourceSampleRate = 44100.0;
            auto buffer = processor.getSamplePool().loadOrGet(file.getFullPathName(), sourceSampleRate);

            const bool stereoRequested = buffer != nullptr
                && (forceStereoImport || buffer->getNumChannels() >= 2
                    || SampleImportMatcher::hasStereoMarker(file.getFileNameWithoutExtension()));

            if (stereoRequested)
            {
                // Match against a virtual stereo layout as well. This lets a
                // file called OH.wav convert the default OH_L/OH_R pair and
                // import in one operation, without a separate settings step.
                auto inferenceLayout = *targetInst;
                DefaultKitFactory::reconfigureAmbienceChannels(
                    inferenceLayout, MicRole::Overhead, AmbienceFormat::Stereo);
                DefaultKitFactory::reconfigureAmbienceChannels(
                    inferenceLayout, MicRole::Room, AmbienceFormat::Stereo);

                auto match = SampleImportMatcher::match(file.getFileNameWithoutExtension(), inferenceLayout);
                if (! match.has_value())
                    match = SampleImportMatcher::match(file.getParentDirectory().getFileName() + " "
                                                       + file.getFileNameWithoutExtension(), inferenceLayout);

                const MicChannelDef* matchedChannel = match.has_value()
                    ? inferenceLayout.findChannel(match->channelId) : nullptr;
                if (matchedChannel != nullptr && matchedChannel->numChannels == 2
                    && isAmbienceRole(matchedChannel->role))
                {
                    const bool committed = commitImportedStereoSample(
                        instrumentId, articulationId, layerIndex, matchedChannel->role,
                        file, buffer, sourceSampleRate);
                    if (onComplete)
                        onComplete({ committed ? 1 : 0, committed ? 1 : 0,
                                     committed ? juce::StringArray() : juce::StringArray(file.getFileName()) });
                    return;
                }

                std::vector<MicRole> stereoDestinationRoles;
                juce::StringArray stereoDestinationNames;
                for (auto role : { MicRole::Overhead, MicRole::Room })
                {
                    auto candidate = *targetInst;
                    if (DefaultKitFactory::reconfigureAmbienceChannels(
                            candidate, role, AmbienceFormat::Stereo))
                    {
                        stereoDestinationRoles.push_back(role);
                        bool alreadyStereo = false;
                        for (auto& channel : targetInst->micChannels)
                            alreadyStereo = alreadyStereo || (channel.role == role && channel.numChannels == 2);

                        if (role == MicRole::Overhead)
                            stereoDestinationNames.add(alreadyStereo
                                ? "Overhead / OH — Single stereo channel"
                                : "Overhead / OH — switch L + R to Single stereo");
                        else
                            stereoDestinationNames.add(alreadyStereo
                                ? "Room — Single stereo channel"
                                : "Room — switch L + R to Single stereo");
                    }
                }

                if (stereoDestinationRoles.size() == 1)
                {
                    const bool committed = commitImportedStereoSample(
                        instrumentId, articulationId, layerIndex, stereoDestinationRoles.front(),
                        file, buffer, sourceSampleRate);
                    if (onComplete)
                        onComplete({ committed ? 1 : 0, committed ? 1 : 0,
                                     committed ? juce::StringArray() : juce::StringArray(file.getFileName()) });
                    return;
                }

                if (stereoDestinationRoles.size() > 1)
                {
                    auto* dialog = new juce::AlertWindow("Import stereo sample",
                        "Choose whether this stereo file belongs to the Overhead or Room channel.",
                        juce::MessageBoxIconType::QuestionIcon);
                    dialog->addComboBox("destination", stereoDestinationNames, "Destination channel");
                    if (auto* box = dialog->getComboBoxComponent("destination"))
                        box->setSelectedId(1, juce::dontSendNotification);
                    dialog->addButton("Import", 1, juce::KeyPress(juce::KeyPress::returnKey));
                    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

                    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
                        [this, dialog, stereoDestinationRoles, instrumentId, articulationId, layerIndex,
                         file, buffer, sourceSampleRate, onComplete](int result)
                        {
                            bool committed = false;
                            if (result == 1)
                            {
                                const int selected = dialog->getComboBoxComponent("destination")->getSelectedId() - 1;
                                if (selected >= 0 && selected < (int) stereoDestinationRoles.size())
                                    committed = commitImportedStereoSample(
                                        instrumentId, articulationId, layerIndex,
                                        stereoDestinationRoles[(size_t) selected], file,
                                        buffer, sourceSampleRate);
                            }

                            if (onComplete)
                                onComplete({ committed ? 1 : 0, committed ? 1 : 0,
                                             committed ? juce::StringArray() : juce::StringArray(file.getFileName()) });
                            dialog->exitModalState(0);
                            delete dialog;
                        }));
                    return;
                }
            }
        }

        // Bulk stereo import: first infer the ambience role from a temporary
        // all-stereo layout, then switch the real instrument before matching.
        // This is what allows a whole folder of "Overheads ST" + "Room ST"
        // files to replace the default OH_L/OH_R + Room_L/Room_R topology.
        auto inferenceLayout = *targetInst;
        DefaultKitFactory::reconfigureAmbienceChannels(
            inferenceLayout, MicRole::Overhead, AmbienceFormat::Stereo);
        DefaultKitFactory::reconfigureAmbienceChannels(
            inferenceLayout, MicRole::Room, AmbienceFormat::Stereo);

        bool needsStereoOverhead = false;
        bool needsStereoRoom = false;
        for (auto& file : files)
        {
            const auto baseName = file.getFileNameWithoutExtension();
            if (! forceStereoImport && ! SampleImportMatcher::hasStereoMarker(baseName))
                continue;

            auto inferred = SampleImportMatcher::match(baseName, inferenceLayout);
            if (! inferred.has_value())
                inferred = SampleImportMatcher::match(
                    file.getParentDirectory().getFileName() + " " + baseName, inferenceLayout);
            const auto* channel = inferred.has_value() ? inferenceLayout.findChannel(inferred->channelId) : nullptr;
            if (channel != nullptr && channel->numChannels == 2)
            {
                needsStereoOverhead = needsStereoOverhead || channel->role == MicRole::Overhead;
                needsStereoRoom = needsStereoRoom || channel->role == MicRole::Room;
            }
        }

        if (needsStereoOverhead)
            DefaultKitFactory::reconfigureAmbienceChannels(
                *targetInst, MicRole::Overhead, AmbienceFormat::Stereo);
        if (needsStereoRoom)
            DefaultKitFactory::reconfigureAmbienceChannels(
                *targetInst, MicRole::Room, AmbienceFormat::Stereo);

        struct PendingFile { juce::File file; juce::String channelId; };
        struct PendingGroup
        {
            juce::String articulationId;
            std::vector<PendingFile> files;
        };
        std::map<juce::String, PendingGroup> byVariation;
        std::vector<juce::String> variationOrder;
        juce::StringArray unmatched;

        for (auto& file : files)
        {
            const juce::String baseName = file.getFileNameWithoutExtension();
            auto match = SampleImportMatcher::match(baseName, *targetInst);
            if (! match.has_value())
                match = SampleImportMatcher::match(file.getParentDirectory().getFileName() + " " + baseName,
                                                   *targetInst);
            if (! match.has_value())
            {
                unmatched.add(file.getFileName());
                continue;
            }

            const auto destinationArticulationId = SampleImportMatcher::targetArticulationId(
                baseName, *targetInst, targetArticulation->id);

            const juce::String groupKey = destinationArticulationId + juce::String::charToString(0x1f)
                                        + match->variationKey;
            if (byVariation.find(groupKey) == byVariation.end())
            {
                variationOrder.push_back(groupKey);
                byVariation[groupKey].articulationId = destinationArticulationId;
            }
            byVariation[groupKey].files.push_back({ file, match->channelId });
        }

        std::sort(variationOrder.begin(), variationOrder.end());

        int filesAssigned = 0;

        for (auto& key : variationOrder)
        {
            auto& group = byVariation[key];
            auto* destinationArticulation = targetInst->findArticulation(group.articulationId);
            if (destinationArticulation == nullptr || layerIndex < 0
                || layerIndex >= (int) destinationArticulation->velocityLayers.size())
                continue;

            RoundRobinSlot newSlot;
            for (auto& pf : group.files)
            {
                double sourceSampleRate = 44100.0;
                auto buffer = processor.getSamplePool().loadOrGet(pf.file.getFullPathName(), sourceSampleRate);
                if (buffer == nullptr)
                    continue;

                SampleRef ref;
                ref.sourcePath = pf.file.getFullPathName();
                ref.buffer = buffer;
                ref.sourceSampleRate = sourceSampleRate;
                newSlot.channelSamples[pf.channelId] = ref;
                ++filesAssigned;
            }
            destinationArticulation->velocityLayers[(size_t) layerIndex].slots.push_back(std::move(newSlot));
        }

        pushToEngine();
        notifyListeners();

        if (onComplete)
            onComplete({ filesAssigned, (int) variationOrder.size(), unmatched });
    });
}

void KitEditController::setChannelFaderDb(const juce::String& micChannelId, float db)
{
    if (auto* inst = getSelectedInstrument())
    {
        inst->channelFaderDb[micChannelId] = db;
        pushToEngine(); // continuous drag: push audio state but skip a full UI rebuild
    }
}

void KitEditController::setChannelFaderDbFor(const juce::String& instrumentId, const juce::String& micChannelId, float db)
{
    if (auto* inst = workingKit.findInstrumentById(instrumentId))
    {
        inst->channelFaderDb[micChannelId] = db;
        pushToEngine();
    }
}

void KitEditController::setChannelPanFor(const juce::String& instrumentId,
                                          const juce::String& micChannelId,
                                          float pan)
{
    if (auto* instrument = workingKit.findInstrumentById(instrumentId))
        if (auto* channel = instrument->findChannel(micChannelId))
            if (channel->numChannels == 1)
            {
                channel->pan = juce::jlimit(-1.0f, 1.0f, pan);
                pushToEngine();
            }
}

void KitEditController::setChannelRoutingFor(const juce::String& instrumentId, const juce::String& micChannelId,
                                              const juce::String& busId)
{
    if (auto* inst = workingKit.findInstrumentById(instrumentId))
    {
        inst->channelRouting[micChannelId] = busId;
        pushToEngine();
        notifyListeners();
    }
}

void KitEditController::setOverheadBusRouting(const juce::String& busId)
{
    workingKit.overheadBusId = busId;
    pushToEngine();
    notifyListeners();
}

void KitEditController::setRoomBusRouting(const juce::String& busId)
{
    workingKit.roomBusId = busId;
    pushToEngine();
    notifyListeners();
}

void KitEditController::toggleChannelMuteFor(const juce::String& instrumentId, const juce::String& micChannelId)
{
    if (auto* inst = workingKit.findInstrumentById(instrumentId))
    {
        inst->channelMuted[micChannelId] = ! inst->isChannelMuted(micChannelId);
        pushToEngine();
        notifyListeners();
    }
}

void KitEditController::toggleChannelSoloFor(const juce::String& instrumentId, const juce::String& micChannelId)
{
    if (auto* inst = workingKit.findInstrumentById(instrumentId))
    {
        inst->channelSolo[micChannelId] = ! inst->isChannelSoloed(micChannelId);
        pushToEngine();
        notifyListeners();
    }
}

void KitEditController::toggleOverheadMute()
{
    workingKit.overheadMuted = ! workingKit.overheadMuted;
    pushToEngine();
    notifyListeners();
}

void KitEditController::toggleOverheadSolo()
{
    workingKit.overheadSolo = ! workingKit.overheadSolo;
    pushToEngine();
    notifyListeners();
}

void KitEditController::toggleRoomMute()
{
    workingKit.roomMuted = ! workingKit.roomMuted;
    pushToEngine();
    notifyListeners();
}

void KitEditController::toggleRoomSolo()
{
    workingKit.roomSolo = ! workingKit.roomSolo;
    pushToEngine();
    notifyListeners();
}

void KitEditController::setVelocityLayerCount(int count)
{
    workingKit.setVelocityLayerCount(count);
    selectedLayerIndex = juce::jlimit(0, juce::jmax(0, workingKit.velocityConfig.getLayerCount() - 1), selectedLayerIndex);
    pushToEngine();
    notifyListeners();
}

void KitEditController::setThreshold(int thresholdIndex, int value)
{
    auto& thresholds = workingKit.velocityConfig.thresholds;
    if (thresholdIndex < 0 || thresholdIndex >= (int) thresholds.size())
        return;

    const int lowerBound = thresholdIndex > 0 ? thresholds[(size_t) thresholdIndex - 1] + 1 : 1;
    const int upperBound = thresholdIndex < (int) thresholds.size() - 1 ? thresholds[(size_t) thresholdIndex + 1] - 1 : 126;
    thresholds[(size_t) thresholdIndex] = juce::jlimit(lowerBound, upperBound, value);

    pushToEngine();
    notifyListeners();
}

void KitEditController::resetVelocityThresholds()
{
    auto& thresholds = workingKit.velocityConfig.thresholds;
    const int n = (int) thresholds.size();
    for (int i = 0; i < n; ++i)
        thresholds[(size_t) i] = (int) juce::jmap((double) (i + 1), 0.0, (double) n + 1.0, 1.0, 126.0);

    pushToEngine();
    notifyListeners();
}

void KitEditController::setMasterVolumeDb(float db) { workingKit.masterDb = db; pushToEngine(); }
void KitEditController::setMasterOverheadDb(float db) { workingKit.masterOverheadDb = db; pushToEngine(); }
void KitEditController::setMasterRoomDb(float db) { workingKit.masterRoomDb = db; pushToEngine(); }
void KitEditController::setMasterTuneSemitones(float semitones) { workingKit.masterTuneSemitones = semitones; pushToEngine(); }

void KitEditController::previewSelectedInstrument(int velocity)
{
    if (velocity < 0)
        velocity = workingKit.velocityConfig.previewVelocityForLayer(selectedLayerIndex);

    if (auto* articulation = getSelectedArticulation())
        if (articulation->midiNote >= 0)
            processor.triggerPreviewNote(articulation->midiNote, juce::jlimit(1, 127, velocity));
}

void KitEditController::createNewKit()
{
    processor.getEngine().cancelMidiLearn();
    workingKit = DefaultKitFactory::createDefaultKit();
    selectedInstrumentId = workingKit.instruments.empty() ? juce::String() : workingKit.instruments.front().id;
    selectedLayerIndex = 0;
    selectedArticulationIndex = 0;
    midiLearnArmedInstrumentId.clear();
    midiLearnArmedArticulationId.clear();
    currentPresetDirectory = juce::File {};
    lastDrumsetExportDirectory = juce::File {};

    pushToEngine();
    processor.getSamplePool().clear();
    notifyListeners();
}

void KitEditController::savePresetAs()
{
    activeChooser = std::make_shared<juce::FileChooser>("Save preset", juce::File(), "*.json");
    const auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting;

    activeChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        activeChooser.reset();
        if (file == juce::File {})
            return;

        if (! file.hasFileExtension("json"))
            file = file.withFileExtension("json");

        file.replaceWithText(Preset::toJsonString(workingKit));
    });
}

void KitEditController::loadPresetViaDialog()
{
    activeChooser = std::make_shared<juce::FileChooser>("Load preset", juce::File(), "*.json");
    const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    activeChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        activeChooser.reset();
        if (! file.existsAsFile())
            return;

        currentPresetDirectory = file.getParentDirectory();
        const auto loadResult = processor.loadKitFromPresetFile(file);
        adoptProcessorKit();

        if (loadResult.missingSamplePaths.isEmpty())
            return;

        activeChooser = std::make_shared<juce::FileChooser>(
            juce::String(loadResult.missingSamplePaths.size())
                + " sample file(s) are missing. Select their parent folder to search recursively.",
            file.getParentDirectory());
        const auto folderFlags = juce::FileBrowserComponent::openMode
                               | juce::FileBrowserComponent::canSelectDirectories;
        juce::WeakReference<KitEditController> safeThis(this);
        activeChooser->launchAsync(folderFlags, [safeThis, file](const juce::FileChooser& folderChooser)
        {
            if (safeThis == nullptr)
                return;

            const auto searchDirectory = folderChooser.getResult();
            safeThis->activeChooser.reset();
            if (! searchDirectory.isDirectory())
                return;

            const auto repairResult = safeThis->processor.loadKitFromPresetFile(file, searchDirectory);
            safeThis->adoptProcessorKit();

            if (repairResult.relinkedSamples == 0)
            {
                juce::NativeMessageBox::showAsync(
                    juce::MessageBoxOptions()
                        .withIconType(juce::MessageBoxIconType::WarningIcon)
                        .withTitle("Samples not found")
                        .withMessage("No unambiguous filename matches were found in the selected folder. "
                                     + juce::String(repairResult.missingSamplePaths.size())
                                     + " sample path(s) are still missing.")
                        .withButton("OK"),
                    nullptr);
                return;
            }

            auto message = juce::String(repairResult.relinkedSamples)
                         + " sample path(s) were repaired and loaded.";
            if (! repairResult.missingSamplePaths.isEmpty())
                message += "\n\n" + juce::String(repairResult.missingSamplePaths.size())
                         + " path(s) are still missing or ambiguous.";
            message += "\n\nUpdate the preset file with the repaired paths?";

            juce::NativeMessageBox::showAsync(
                juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::QuestionIcon)
                    .withTitle("Sample paths repaired")
                    .withMessage(message)
                    .withButton("Update preset")
                    .withButton("Later"),
                [safeThis, file](int result)
                {
                    if (result == 0 && safeThis != nullptr)
                    {
                        const bool saved = file.replaceWithText(Preset::toJsonString(safeThis->workingKit));
                        if (! saved)
                            juce::NativeMessageBox::showAsync(
                                juce::MessageBoxOptions()
                                    .withIconType(juce::MessageBoxIconType::WarningIcon)
                                    .withTitle("Preset could not be updated")
                                    .withMessage("The repaired kit is loaded, but the preset file could not be written.")
                                    .withButton("OK"),
                                nullptr);
                    }
                });
        });
    });
}

void KitEditController::exportDrumsetViaDialog(DrumsetExportCallback completionCallback)
{
    if (drumsetExportInProgress)
    {
        DrumsetExporter::Result result;
        result.errorMessage = "A drumset export is already running.";
        if (completionCallback)
            completionCallback(std::move(result));
        return;
    }

    activeChooser = std::make_shared<juce::FileChooser>(
        "Select a parent folder for the portable drumset export", juce::File());
    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectDirectories;

    activeChooser->launchAsync(flags, [this, completion = std::move(completionCallback)](const juce::FileChooser& fc) mutable
    {
        const auto parentDirectory = fc.getResult();
        activeChooser.reset();
        if (! parentDirectory.isDirectory())
            return;

        const auto baseName = DrumsetExporter::suggestedDirectoryName(workingKit.kitName);
        const auto packageDirectory = parentDirectory.getNonexistentChildFile(baseName, {}, true);
        startDrumsetExport(packageDirectory, false, std::move(completion));
    });
}

void KitEditController::updateDrumsetExportViaDialog(DrumsetExportCallback completionCallback)
{
    if (drumsetExportInProgress)
    {
        DrumsetExporter::Result result;
        result.errorMessage = "A drumset export is already running.";
        if (completionCallback)
            completionCallback(std::move(result));
        return;
    }

    const auto initialDirectory = lastDrumsetExportDirectory.isDirectory()
        ? lastDrumsetExportDirectory : juce::File();
    activeChooser = std::make_shared<juce::FileChooser>(
        "Select an existing Swarm drumset export to update", initialDirectory);
    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectDirectories;

    activeChooser->launchAsync(flags, [this, completion = std::move(completionCallback)](const juce::FileChooser& fc) mutable
    {
        const auto packageDirectory = fc.getResult();
        activeChooser.reset();
        if (! packageDirectory.isDirectory())
            return;

        startDrumsetExport(packageDirectory, true, std::move(completion));
    });
}

void KitEditController::startDrumsetExport(const juce::File& packageDirectory,
                                            bool updateExisting,
                                            DrumsetExportCallback completionCallback)
{
    drumsetExportInProgress = true;
    auto kitSnapshot = workingKit;
    const auto sourceBaseDirectory = currentPresetDirectory;
    juce::WeakReference<KitEditController> safeThis(this);
    auto launchFailureCallback = completionCallback;

    const bool launched = juce::Thread::launch(
        [safeThis, exportedKit = std::move(kitSnapshot), packageDirectory,
         sourceBaseDirectory, updateExisting, completion = std::move(completionCallback)]() mutable
        {
            auto exportResult = DrumsetExporter::exportKit(
                exportedKit, packageDirectory, sourceBaseDirectory, updateExisting);

            juce::MessageManager::callAsync(
                [safeThis, completedResult = std::move(exportResult),
                 uiCompletion = std::move(completion)]() mutable
                {
                    if (safeThis == nullptr)
                        return;

                    safeThis->drumsetExportInProgress = false;
                    if (completedResult.success)
                        safeThis->lastDrumsetExportDirectory = completedResult.packageDirectory;
                    if (uiCompletion)
                        uiCompletion(std::move(completedResult));
                });
        });

    if (! launched)
    {
        drumsetExportInProgress = false;
        DrumsetExporter::Result result;
        result.packageDirectory = packageDirectory;
        result.errorMessage = "The background export thread could not be started.";
        if (launchFailureCallback)
            launchFailureCallback(std::move(result));
    }
}

void KitEditController::adoptProcessorKit()
{
    if (auto kit = processor.getEngine().getKit())
        workingKit = *kit;

    if (workingKit.findInstrumentById(selectedInstrumentId) == nullptr && ! workingKit.instruments.empty())
        selectedInstrumentId = workingKit.instruments.front().id;

    selectedLayerIndex = juce::jlimit(0, juce::jmax(0, workingKit.velocityConfig.getLayerCount() - 1), selectedLayerIndex);
    selectedArticulationIndex = 0;
    notifyListeners();
}

void KitEditController::pushToEngine()
{
    processor.getEngine().setKit(std::make_shared<Kit>(workingKit));
}

void KitEditController::notifyListeners()
{
    // Dispatched async and guarded by a WeakReference rather than called
    // synchronously: listeners commonly respond by rebuilding (destroying and
    // recreating) their own child components — including, in the common case,
    // the very component whose event handler is on the call stack right now
    // (e.g. a pad's mouseUp calling selectInstrument() while that pad is
    // still executing). Deleting a Component while it's still running one of
    // its own callbacks is a use-after-free. Deferring to the next message
    // loop iteration lets that callback finish and unwind first.
    juce::WeakReference<KitEditController> safeThis(this);
    juce::MessageManager::callAsync([safeThis]
    {
        if (safeThis != nullptr)
            safeThis->listeners.call([](Listener& l) { l.kitEditorStateChanged(); });
    });
}

void KitEditController::timerCallback()
{
    if (auto result = processor.getEngine().consumeMidiLearnResult())
    {
        workingKit.setArticulationMidiNote(result->instrumentId, result->articulationId, result->midiNote, true);
        if (midiLearnArmedInstrumentId == result->instrumentId
            && midiLearnArmedArticulationId == result->articulationId)
        {
            midiLearnArmedInstrumentId.clear();
            midiLearnArmedArticulationId.clear();
        }

        pushToEngine();
        notifyListeners();
    }
}

juce::String KitEditController::makeUniqueId(const juce::String& baseName) const
{
    auto slug = baseName.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789");
    if (slug.isEmpty())
        slug = "instrument";

    auto candidate = slug;
    int suffix = 2;
    while (workingKit.findInstrumentById(candidate) != nullptr)
        candidate = slug + juce::String(suffix++);

    return candidate;
}

} // namespace dsl::ui
