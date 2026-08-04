#pragma once

#include "Kit.h"
#include "VoicePool.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace dsl
{

/** The real-time playback engine: turns incoming MIDI note-ons into voice
    triggers (velocity layer -> round-robin slot -> one voice per mic channel)
    and renders them into the plugin's output buses.

    Concurrency contract: once installed via setKit(), a Kit is treated as
    read-only by the audio thread — the only field the audio thread mutates is
    VelocityLayer::lastPlayedSlot (private runtime state, never touched by the
    UI/message thread). Any edit coming from the UI (fader, routing, sample
    assignment, added instrument, preset load, ...) must build a new/modified
    Kit on the message thread and hand it to setKit(), rather than mutating the
    live Kit in place. This keeps the audio thread free of locks around
    per-sample work; only the pointer swap itself is guarded, and only ever
    happens on preset/kit changes, not per note.
*/
class DrumEngine
{
public:
    explicit DrumEngine(int maxVoices = 256);

    void prepare(double sampleRate, int maxBlockSize);

    void setKit(std::shared_ptr<Kit> newKit);
    std::shared_ptr<Kit> getKit() const;

    /** Audio-thread entry point. busBuffers must be indexed to match the
        installed Kit's BusRoutingTable order (see docs/01-architecture.md
        section 1.4); PluginProcessor is responsible for building that array
        from the host's actual output buses each block.
    */
    void processBlock(const juce::MidiBuffer& midiMessages,
                       const std::vector<juce::AudioBuffer<float>*>& busBuffers,
                       int numSamples);

    int getNumActiveVoices() const { return voicePool.getNumActiveVoices(); }

    /** Arms MIDI Learn for one instrument: the next note-on received by
        processBlock (from host MIDI or a UI-triggered preview note) is
        captured as that instrument's new trigger note instead of being
        played, and becomes available via consumeMidiLearnResult(). The
        caller (UI) is responsible for actually reassigning the note on the
        Kit once it has the result — this only captures the incoming note.
    */
    void beginMidiLearn(const juce::String& instrumentId, const juce::String& articulationId);
    void cancelMidiLearn();
    bool isMidiLearnActive() const { return midiLearnActive.load(std::memory_order_acquire); }

    struct MidiLearnResult
    {
        juce::String instrumentId;
        juce::String articulationId;
        int midiNote = -1;
    };

    /** Message-thread only. Returns and clears the pending result, if any. */
    std::optional<MidiLearnResult> consumeMidiLearnResult();

    /** Millisecond timestamp (juce::Time::getMillisecondCounter() timebase) of
        the most recent note-on that resolved to an instrument on this MIDI
        note, or 0 if none yet. Written from the audio thread (lock-free),
        polled from the UI thread (e.g. by KitStageView) to drive a brief
        "this pad was hit" animation. Registers regardless of mute/solo, since
        it's a visual "a MIDI hit landed here" indicator, not an audio one.
    */
    juce::uint32 getLastHitTimeMs(int midiNote) const
    {
        return midiNote >= 0 && midiNote < 128 ? lastHitTimeMs[(size_t) midiNote].load(std::memory_order_relaxed) : 0;
    }

private:
    void triggerNoteOn(Kit& kitRef, int midiNote, int velocity);

    mutable std::mutex kitMutex;
    std::shared_ptr<Kit> kit;

    VoicePool voicePool;
    juce::Random rng;
    double sampleRate = 44100.0;

    std::atomic<bool> midiLearnActive { false };
    mutable std::mutex midiLearnMutex;
    juce::String midiLearnInstrumentId;
    juce::String midiLearnArticulationId;
    std::optional<MidiLearnResult> midiLearnResult;

    std::array<std::atomic<juce::uint32>, 128> lastHitTimeMs {};
};

} // namespace dsl
