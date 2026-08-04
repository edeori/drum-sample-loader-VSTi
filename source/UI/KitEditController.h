#pragma once

#include "../PluginProcessor.h"
#include "../Engine/DefaultKitFactory.h"
#include "../Engine/DrumsetExporter.h"
#include "../Engine/Kit.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace dsl::ui
{

/** Mediates between the UI components and the audio engine.
    Owns a "working copy" of the Kit that the UI reads and edits directly;
    every mutation clones it and hands it to DrumEngine::setKit(), matching
    the concurrency contract documented on DrumEngine (the audio thread never
    sees a Kit being mutated in place). Listeners are notified after every
    change so components can just re-read the working copy and refresh.
*/
class KitEditController : private juce::Timer
{
public:
    explicit KitEditController(DrumSampleLoaderAudioProcessor& processorToUse);
    ~KitEditController() override;

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void kitEditorStateChanged() = 0;
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    // ---- Read access ----
    Kit& getWorkingKit() { return workingKit; }
    const Kit& getWorkingKit() const { return workingKit; }
    juce::String getSelectedInstrumentId() const { return selectedInstrumentId; }
    Instrument* getSelectedInstrument();
    InstrumentArticulation* getSelectedArticulation();
    int getSelectedArticulationIndex() const { return selectedArticulationIndex; }
    int getSelectedLayerIndex() const { return selectedLayerIndex; }
    int getOrphanedSampleCount() const;
    bool isMidiLearnArmedFor(const juce::String& instrumentId, const juce::String& articulationId) const
    {
        return midiLearnArmedInstrumentId == instrumentId && midiLearnArmedArticulationId == articulationId;
    }

    // ---- Selection ----
    void selectInstrument(const juce::String& instrumentId);
    void selectArticulation(int articulationIndex);
    void selectLayer(int layerIndex);

    // ---- Instrument management ----
    void addInstrument(const juce::String& name, InstrumentCategory category, int midiNote,
                        AmbienceFormat overheadFormat = AmbienceFormat::MonoPair,
                        AmbienceFormat roomFormat = AmbienceFormat::MonoPair);
    void removeInstrument(const juce::String& instrumentId);
    void renameInstrument(const juce::String& instrumentId, const juce::String& newName);
    void toggleMute(const juce::String& instrumentId);
    void toggleSolo(const juce::String& instrumentId);

    Kit::SetNoteResult setInstrumentMidiNote(const juce::String& instrumentId, int midiNote, bool force);
    Kit::SetNoteResult setArticulationMidiNote(const juce::String& instrumentId,
                                               const juce::String& articulationId,
                                               int midiNote, bool force);
    void beginMidiLearn(const juce::String& instrumentId, const juce::String& articulationId);
    void cancelMidiLearn();

    // ---- Sample editing (selected instrument, selected layer) ----
    void addRoundRobinSlot();
    void removeRoundRobinSlot(int slotIndex);
    void assignSampleToCell(const juce::String& micChannelId, int slotIndex);
    void clearSampleCell(const juce::String& micChannelId, int slotIndex);
    bool setSelectedAmbienceFormats(AmbienceFormat overheadFormat, AmbienceFormat roomFormat);

    /** Bulk-import: lets the user multi-select a whole batch of sample files
        at once (e.g. a folder from a commercial multi-mic pack) instead of
        loading one cell at a time. Files are matched to this instrument's mic
        channels from both conventional "{variation}-{MicPosition}.wav" names
        and standalone stereo ambience names such as OH.wav, Room.wav,
        Snare_OH_01.wav or Snare-Room-Stereo-01.wav. Matching files are grouped
        into round-robin slots by their shared instrument/take identifier.
        Files that can't be confidently matched to a channel are left alone —
        reported back via onComplete for the user to assign by hand. */
    struct SampleImportResult
    {
        int filesAssigned = 0;
        int slotsCreated = 0;
        juce::StringArray unmatchedFileNames;
    };
    void importSamplesToSelectedInstrument(bool forceStereoImport,
                                           std::function<void(SampleImportResult)> onComplete);

    // ---- Mixing (per instrument, selected instrument — pure balance, no routing) ----
    void setChannelFaderDb(const juce::String& micChannelId, float db);

    // ---- Global Mixer: output routing lives here, not per instrument in the
    //      Kit Editor, since overhead/room mics are shared across the whole
    //      kit and routing needs to reach any instrument regardless of which
    //      one is currently selected (docs/04-mixing-routing.md). ----
    void setChannelFaderDbFor(const juce::String& instrumentId, const juce::String& micChannelId, float db);
    void setChannelPanFor(const juce::String& instrumentId, const juce::String& micChannelId, float pan);
    void setChannelRoutingFor(const juce::String& instrumentId, const juce::String& micChannelId, const juce::String& busId);
    void setOverheadBusRouting(const juce::String& busId);
    void setRoomBusRouting(const juce::String& busId);
    void toggleChannelMuteFor(const juce::String& instrumentId, const juce::String& micChannelId);
    void toggleChannelSoloFor(const juce::String& instrumentId, const juce::String& micChannelId);
    void toggleOverheadMute();
    void toggleOverheadSolo();
    void toggleRoomMute();
    void toggleRoomSolo();

    // ---- Global velocity config ----
    void setVelocityLayerCount(int count);
    void setThreshold(int thresholdIndex, int value);
    void resetVelocityThresholds();

    // ---- Master ----
    void setMasterVolumeDb(float db);
    void setMasterOverheadDb(float db);
    void setMasterRoomDb(float db);
    void setMasterTuneSemitones(float semitones);

    // ---- Transport / IO ----
    /** A negative velocity previews the centre of the currently selected
        velocity layer; explicit values are used by the interactive kit. */
    void previewSelectedInstrument(int velocity = -1);
    void createNewKit();
    void savePresetAs();
    void loadPresetViaDialog();
    using DrumsetExportCallback = std::function<void(DrumsetExporter::Result)>;
    void exportDrumsetViaDialog(DrumsetExportCallback onComplete);
    void updateDrumsetExportViaDialog(DrumsetExportCallback onComplete);

    DrumSampleLoaderAudioProcessor& getProcessor() { return processor; }
    std::vector<BusDef> getAvailableBuses() const { return workingKit.busRouting.buses; }

private:
    bool commitImportedSample(const juce::String& instrumentId, const juce::String& articulationId,
                              int layerIndex, const juce::String& channelId, const juce::File& file,
                              std::shared_ptr<const juce::AudioBuffer<float>> buffer, double sourceSampleRate);
    bool commitImportedStereoSample(const juce::String& instrumentId, const juce::String& articulationId,
                                    int layerIndex, MicRole destinationRole, const juce::File& file,
                                    std::shared_ptr<const juce::AudioBuffer<float>> buffer, double sourceSampleRate);
    void adoptProcessorKit();
    void startDrumsetExport(const juce::File& packageDirectory, bool updateExisting,
                            DrumsetExportCallback onComplete);
    void pushToEngine();
    void notifyListeners();
    void timerCallback() override;
    juce::String makeUniqueId(const juce::String& baseName) const;

    DrumSampleLoaderAudioProcessor& processor;
    Kit workingKit;
    juce::String selectedInstrumentId;
    int selectedLayerIndex = 0;
    int selectedArticulationIndex = 0;
    juce::String midiLearnArmedInstrumentId;
    juce::String midiLearnArmedArticulationId;
    juce::File currentPresetDirectory;
    juce::File lastDrumsetExportDirectory;
    bool drumsetExportInProgress = false;

    std::shared_ptr<juce::FileChooser> activeChooser;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_WEAK_REFERENCEABLE(KitEditController)
};

} // namespace dsl::ui
