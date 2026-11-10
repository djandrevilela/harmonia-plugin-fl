#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <functional>
#include <memory>

#include "PluginProcessor.h"

// Teclado que mostra escala, notas do acorde, sugestões de próxima nota e teclas premidas.
class KeyboardView : public juce::Component {
public:
    explicit KeyboardView(juce::MidiKeyboardState& s) : state(s) {}
    void setInfo(const harmonia::Scale& sc, const std::array<bool, 12>& chordMask, std::vector<harmonia::NoteSuggestion> sug,
                 std::array<bool, 128> held);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    static constexpr int firstNote = 36, lastNote = 95;

private:
    int noteAt(juce::Point<float>) const;
    juce::Rectangle<float> rectFor(int note) const;
    static bool isBlack(int n) { int p = n % 12; return p == 1 || p == 3 || p == 6 || p == 8 || p == 10; }

    juce::MidiKeyboardState& state;
    harmonia::Scale scale;
    std::array<bool, 12> chordMask{};
    std::vector<harmonia::NoteSuggestion> suggestions;
    std::array<bool, 128> heldNotes{};
    int mouseNote = -1;
};

class ChordSlot : public juce::Component {
public:
    ChordSlot(int index, std::function<void(int)> onAudition, std::function<void(int, const std::string&)> onToken);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void setContent(const juce::String& name, const juce::String& notes, const std::string& token,
                    const std::vector<std::string>& palette, bool active);

    juce::ComboBox tokenBox;
    juce::Label nameLabel, notesLabel;

private:
    int idx;
    bool isActive = false;
    std::vector<std::string> pal;
    std::function<void(int)> audition;
    std::function<void(int, const std::string&)> tokenChanged;
};

class MidiDragHandle : public juce::Component {
public:
    std::function<juce::File()> makeFile;
    void paint(juce::Graphics&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override { dragging = false; }

private:
    bool dragging = false;
};

class HarmoniaEditor : public juce::AudioProcessorEditor,
                       private juce::ChangeListener,
                       private juce::Timer,
                       public juce::FileDragAndDropTarget {
public:
    explicit HarmoniaEditor(HarmoniaAudioProcessor&);
    ~HarmoniaEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int, int) override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override { refresh(); }
    void timerCallback() override;
    void refresh();
    void showReport(const harmonia::Report&, const juce::String& title);
    void analyzeNotes(const std::vector<harmonia::NoteEvent>&, const juce::String& title);
    void useDetectedKey();

    HarmoniaAudioProcessor& proc;
    juce::LookAndFeel_V4 lnf;

    juce::ComboBox keyBox, scaleBox, styleBox, richBox, spreadBox, barsBox, snapBox;
    juce::Label keyL, scaleL, styleL, richL, spreadL, barsL, snapL;
    juce::ToggleButton bassTg, playTg, soundTg;
    juce::TextButton prevBtn, genBtn, nextBtn, addBtn, remBtn, exportBtn, analyzeNotesBtn, analyzeProgBtn, clearBtn, useKeyBtn;
    MidiDragHandle dragHandle;
    juce::OwnedArray<ChordSlot> slots;
    juce::TextEditor explainBox, reportBox;
    juce::Label statusLabel;
    KeyboardView keyboard;
    std::unique_ptr<juce::FileChooser> chooser;

    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<CA>> comboAtt;
    std::vector<std::unique_ptr<BA>> buttonAtt;

    std::optional<harmonia::KeyGuess> detectedKey;
    int lastActive = -2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmoniaEditor)
};
