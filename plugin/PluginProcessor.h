#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>
#include <memory>

#include "HarmonyChecker.h"
#include "NextNote.h"
#include "PreviewSynth.h"
#include "Progressions.h"
#include "Theory.h"
#include "VoiceLeading.h"

namespace ids {
constexpr const char* keyRoot = "keyRoot";
constexpr const char* scaleType = "scaleType";
constexpr const char* style = "style";
constexpr const char* richness = "richness";
constexpr const char* spread = "spread";
constexpr const char* bars = "barsPerChord";
constexpr const char* bass = "includeBass";
constexpr const char* playProg = "playProgression";
constexpr const char* sound = "internalSound";
constexpr const char* snap = "snapMode";
}  // namespace ids

// Dados imutáveis partilhados entre a thread de mensagens e a de áudio.
struct PlaybackData {
    std::vector<harmonia::Chord> chords;
    harmonia::VoicedProgression voiced;
    std::vector<std::string> names, romans, functions;
    std::vector<std::vector<int>> allNotes;  // baixo (se existir) + vozes superiores
    double barsPerChord = 1.0;
    bool flats = false;
    int totalMovement = 0;
};

class HarmoniaAudioProcessor : public juce::AudioProcessor,
                               public juce::ChangeBroadcaster,
                               private juce::AsyncUpdater,
                               private juce::AudioProcessorValueTreeState::Listener {
public:
    HarmoniaAudioProcessor();
    ~HarmoniaAudioProcessor() override;

    // AudioProcessor
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Harmonia"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // ---- API para o editor (thread de mensagens) ----
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    harmonia::Scale getScale() const;
    bool isMinorMode() const;
    harmonia::Richness effectiveRichness() const;
    int styleIndex() const;

    void generate(int variationDelta);  // 0 = recarregar; +1/-1 = variação seguinte/anterior
    std::vector<std::string> getTokens() const;
    void setToken(int slot, const std::string& token);
    void addSlot();
    void removeSlot();
    juce::String getWhyText() const;
    int variationNumber() const { return variation; }

    std::shared_ptr<const PlaybackData> getView() const;
    void auditionChord(int index);
    int getFocusChord() const { return focusChord.load(); }
    int getCurrentChordIndex() const { return playingChord.load(); }
    bool isHeld(int note) const { return held[(size_t) note].load(); }

    std::vector<int> getRecentOutputNotes(int maxCount) const;
    std::vector<harmonia::NoteEvent> getRecordedNotes() const;
    void clearRecorded();

    bool writeMidiFile(const juce::File& f) const;
    juce::String suggestedFileName() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged(const juce::String& id, float) override;
    void handleAsyncUpdate() override;
    void loadTemplate();
    void rebuild();
    void switchChord(juce::MidiBuffer&, int offset, int idx);
    void releaseProgNotes(juce::MidiBuffer&, int offset);
    void releaseAudition(juce::MidiBuffer&, int offset);
    int snapNote(int note, int mode, const harmonia::Scale& sc) const;

    // modelo (thread de mensagens)
    juce::CriticalSection modelLock;
    std::vector<harmonia::ChordStep> steps;
    std::string whyText;
    int variation = 0;
    bool lastMinor = true;
    std::atomic<bool> repick{false};

    // publicação para a thread de áudio
    mutable juce::SpinLock dataLock;
    std::shared_ptr<const PlaybackData> published;
    std::vector<std::shared_ptr<const PlaybackData>> graveyard;
    std::shared_ptr<const PlaybackData> audioData;

    // estado do áudio
    juce::Synthesiser synth;
    juce::MidiBuffer outMidi;
    std::vector<int> progNotes, audNotes;
    int audSamplesLeft = 0;
    long long lastAbsChord = -1;
    bool progWasPlaying = false;
    double internalBeat = 0.0;
    int lastInputNote = -1;
    std::array<std::array<int, 128>, 16> mapped{};
    std::atomic<int> auditionRequest{-1};
    std::atomic<int> focusChord{-1};
    std::atomic<int> playingChord{-1};
    std::array<std::atomic<bool>, 128> held;

    struct Rec { int midi; int out; double start; double end; bool open; };
    mutable juce::SpinLock recLock;
    std::vector<Rec> rec;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmoniaAudioProcessor)
};
