#include "PluginEditor.h"

#include "Util.h"

using namespace harmonia;

namespace {
const juce::Colour kBg(0xff12141c), kPanel(0xff1c2030), kAccent(0xff4fd1c5), kWarm(0xfff6ad55), kText(0xffe2e8f0), kMuted(0xff8a94a8);
}

// =============================================================== KeyboardView
void KeyboardView::setInfo(const Scale& sc, const std::array<bool, 12>& cm, std::vector<NoteSuggestion> sug, std::array<bool, 128> held) {
    scale = sc; chordMask = cm; suggestions = std::move(sug); heldNotes = held;
    repaint();
}

juce::Rectangle<float> KeyboardView::rectFor(int note) const {
    int whites = 0;
    for (int n = firstNote; n <= lastNote; ++n) if (!isBlack(n)) ++whites;
    const float ww = getWidth() / (float) whites, h = (float) getHeight();
    int idx = 0;
    for (int n = firstNote; n < note; ++n) if (!isBlack(n)) ++idx;
    if (!isBlack(note)) return {idx * ww, 0, ww, h};
    return {idx * ww - ww * 0.3f, 0, ww * 0.6f, h * 0.62f};
}

int KeyboardView::noteAt(juce::Point<float> p) const {
    for (int n = firstNote; n <= lastNote; ++n) if (isBlack(n) && rectFor(n).contains(p)) return n;
    for (int n = firstNote; n <= lastNote; ++n) if (!isBlack(n) && rectFor(n).contains(p)) return n;
    return -1;
}

void KeyboardView::paint(juce::Graphics& g) {
    auto drawKey = [&](int n, bool black) {
        auto r = rectFor(n).reduced(black ? 0.5f : 1.0f, 0);
        const int pc = n % 12;
        const bool inScale = scale.containsPc(pc);
        juce::Colour base = black ? (inScale ? juce::Colour(0xff2b3a55) : juce::Colour(0xff0b0c10))
                                  : (inScale ? juce::Colour(0xffdfe7f3) : juce::Colour(0xff9aa1ad));
        if (heldNotes[(size_t) n]) base = kAccent;
        g.setColour(base);
        g.fillRoundedRectangle(r, 3.0f);
        // sugestões
        for (size_t i = 0; i < suggestions.size(); ++i)
            if (suggestions[i].midi % 12 == pc && std::abs(suggestions[i].midi - n) < 12 && n == suggestions[i].midi) {
                float a = juce::jlimit(0.35f, 1.0f, (float) (suggestions[i].score / 7.0));
                g.setColour(juce::Colour(0xff48bb78).withAlpha(a));
                g.fillRoundedRectangle(r.reduced(2.0f), 3.0f);
                g.setColour(juce::Colours::black);
                g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
                g.drawText(juce::String((int) i + 1), r.withTop(r.getBottom() - 38).withHeight(18), juce::Justification::centred);
            }
        if (chordMask[(size_t) pc]) {
            g.setColour(kWarm);
            g.fillEllipse(r.getCentreX() - 4, r.getBottom() - 14, 8, 8);
        }
        if (pc == scale.root) {
            g.setColour(juce::Colour(0xffe53e3e));
            g.fillEllipse(r.getCentreX() - 2.5f, r.getBottom() - 26, 5, 5);
        }
        if (!black && pc == 0) {
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.setFont(juce::FontOptions(10.0f));
            g.drawText("C" + juce::String(n / 12 - 1), r.withTop(r.getBottom() - 12), juce::Justification::centred);
        }
    };
    for (int n = firstNote; n <= lastNote; ++n) if (!isBlack(n)) drawKey(n, false);
    for (int n = firstNote; n <= lastNote; ++n) if (isBlack(n)) drawKey(n, true);
}

void KeyboardView::mouseDown(const juce::MouseEvent& e) {
    mouseNote = noteAt(e.position);
    if (mouseNote >= 0) state.noteOn(1, mouseNote, 0.8f);
}
void KeyboardView::mouseUp(const juce::MouseEvent&) {
    if (mouseNote >= 0) state.noteOff(1, mouseNote, 0.0f);
    mouseNote = -1;
}

// =============================================================== ChordSlot
ChordSlot::ChordSlot(int index, std::function<void(int)> onAudition, std::function<void(int, const std::string&)> onToken)
    : idx(index), audition(std::move(onAudition)), tokenChanged(std::move(onToken)) {
    addAndMakeVisible(tokenBox);
    addAndMakeVisible(nameLabel);
    addAndMakeVisible(notesLabel);
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    nameLabel.setInterceptsMouseClicks(false, false);
    notesLabel.setJustificationType(juce::Justification::centred);
    notesLabel.setFont(juce::FontOptions(12.0f));
    notesLabel.setColour(juce::Label::textColourId, kMuted);
    notesLabel.setInterceptsMouseClicks(false, false);
    tokenBox.onChange = [this] {
        int i = tokenBox.getSelectedItemIndex();
        if (i >= 0 && i < (int) pal.size()) tokenChanged(idx, pal[(size_t) i]);
    };
}

void ChordSlot::setContent(const juce::String& name, const juce::String& notes, const std::string& token,
                           const std::vector<std::string>& palette, bool active) {
    if (pal != palette) {
        pal = palette;
        tokenBox.clear(juce::dontSendNotification);
        int id = 1;
        for (auto& t : pal) tokenBox.addItem(U8(prettyToken(t)), id++);
    }
    auto it = std::find(pal.begin(), pal.end(), token);
    tokenBox.setSelectedItemIndex(it == pal.end() ? -1 : (int) (it - pal.begin()), juce::dontSendNotification);
    nameLabel.setText(name, juce::dontSendNotification);
    notesLabel.setText(notes, juce::dontSendNotification);
    isActive = active;
    repaint();
}

void ChordSlot::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(2);
    g.setColour(isActive ? kAccent.withAlpha(0.22f) : kPanel);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(isActive ? kAccent : juce::Colour(0xff2d3348));
    g.drawRoundedRectangle(r, 8.0f, isActive ? 2.0f : 1.0f);
}

void ChordSlot::resized() {
    auto r = getLocalBounds().reduced(8);
    tokenBox.setBounds(r.removeFromTop(24));
    r.removeFromTop(6);
    nameLabel.setBounds(r.removeFromTop(38));
    notesLabel.setBounds(r.removeFromTop(18));
}

void ChordSlot::mouseDown(const juce::MouseEvent&) { audition(idx); }

// =============================================================== MidiDragHandle
void MidiDragHandle::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(1);
    g.setColour(kAccent.withAlpha(0.18f));
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(kAccent);
    g.drawRoundedRectangle(r, 8.0f, 1.5f);
    g.setColour(kText);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(TR("Arrastar MIDI  \xE2\x96\xB8  FL Studio"), getLocalBounds(), juce::Justification::centred);
}

void MidiDragHandle::mouseDrag(const juce::MouseEvent& e) {
    if (dragging || e.getDistanceFromDragStart() < 6 || !makeFile) return;
    auto f = makeFile();
    if (!f.existsAsFile()) return;
    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles({f.getFullPathName()}, false, this, [this] { dragging = false; });
}

// =============================================================== Editor
HarmoniaEditor::HarmoniaEditor(HarmoniaAudioProcessor& p) : AudioProcessorEditor(&p), proc(p), keyboard(p.keyboardState) {
    lnf.setColourScheme(juce::LookAndFeel_V4::getMidnightColourScheme());
    lnf.setColour(juce::ResizableWindow::backgroundColourId, kBg);
    setLookAndFeel(&lnf);
    setSize(1020, 740);

    auto setupCombo = [&](juce::ComboBox& cb, juce::Label& lab, const juce::String& text, const char* id) {
        addAndMakeVisible(cb);
        lab.setText(text, juce::dontSendNotification);
        lab.setColour(juce::Label::textColourId, kMuted);
        lab.setFont(juce::FontOptions(12.0f));
        lab.attachToComponent(&cb, false);
        addAndMakeVisible(lab);
        comboAtt.push_back(std::make_unique<CA>(proc.apvts, id, cb));
    };
    setupCombo(keyBox, keyL, TR("Tom"), ids::keyRoot);
    setupCombo(scaleBox, scaleL, TR("Escala"), ids::scaleType);
    setupCombo(styleBox, styleL, TR("Estilo"), ids::style);
    setupCombo(richBox, richL, TR("Riqueza dos acordes"), ids::richness);
    setupCombo(spreadBox, spreadL, "Voicing", ids::spread);
    setupCombo(barsBox, barsL, TR("Duração por acorde"), ids::bars);
    setupCombo(snapBox, snapL, TR("Scale lock / correção de notas (MIDI de entrada)"), ids::snap);

    auto setupToggle = [&](juce::ToggleButton& t, const juce::String& text, const char* id) {
        t.setButtonText(text);
        addAndMakeVisible(t);
        buttonAtt.push_back(std::make_unique<BA>(proc.apvts, id, t));
    };
    setupToggle(bassTg, TR("Baixo (raiz)"), ids::bass);
    setupToggle(playTg, TR("Tocar progressão com o FL (saída MIDI)"), ids::playProg);
    setupToggle(soundTg, TR("Som interno de pré-escuta"), ids::sound);

    auto setupBtn = [&](juce::TextButton& b, const juce::String& text, std::function<void()> fn) {
        b.setButtonText(text);
        b.onClick = std::move(fn);
        addAndMakeVisible(b);
    };
    setupBtn(prevBtn, TR("\xE2\x97\x82 Variação"), [this] { proc.generate(-1); });
    setupBtn(genBtn, TR("Gerar"), [this] { proc.generate(0); });
    setupBtn(nextBtn, TR("Variação \xE2\x96\xB8"), [this] { proc.generate(+1); });
    setupBtn(addBtn, "+", [this] { proc.addSlot(); });
    setupBtn(remBtn, TR("\xE2\x88\x92"), [this] { proc.removeSlot(); });
    setupBtn(exportBtn, TR("Exportar .mid"), [this] {
        chooser = std::make_unique<juce::FileChooser>(TR("Guardar progressão em MIDI"),
                                                      juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                          .getChildFile(proc.suggestedFileName() + ".mid"),
                                                      "*.mid");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                                 juce::FileBrowserComponent::warnAboutOverwriting,
                             [this](const juce::FileChooser& fc) {
                                 auto f = fc.getResult();
                                 if (f != juce::File()) proc.writeMidiFile(f.withFileExtension("mid"));
                             });
    });
    setupBtn(analyzeNotesBtn, TR("Analisar notas tocadas"), [this] {
        analyzeNotes(proc.getRecordedNotes(), TR("Notas tocadas"));
    });
    setupBtn(analyzeProgBtn, TR("Analisar progressão"), [this] {
        auto v = proc.getView();
        std::vector<NoteEvent> ev;
        if (v)
            for (size_t c = 0; c < v->allNotes.size(); ++c)
                for (int n : v->allNotes[c]) ev.push_back({n, (double) c * v->barsPerChord * 4.0, v->barsPerChord * 4.0});
        analyzeNotes(ev, TR("Progressão"));
    });
    setupBtn(clearBtn, "Limpar", [this] { proc.clearRecorded(); reportBox.clear(); detectedKey.reset(); useKeyBtn.setEnabled(false); });
    setupBtn(useKeyBtn, TR("Usar tom detetado"), [this] { useDetectedKey(); });
    useKeyBtn.setEnabled(false);

    dragHandle.makeFile = [this] {
        auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(proc.suggestedFileName() + ".mid");
        return proc.writeMidiFile(f) ? f : juce::File();
    };
    addAndMakeVisible(dragHandle);

    for (auto* tb : {&explainBox, &reportBox}) {
        tb->setMultiLine(true, true);
        tb->setReadOnly(true);
        tb->setCaretVisible(false);
        tb->setColour(juce::TextEditor::backgroundColourId, kPanel);
        tb->setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2d3348));
        tb->setColour(juce::TextEditor::textColourId, kText);
        addAndMakeVisible(*tb);
    }
    statusLabel.setColour(juce::Label::textColourId, kMuted);
    statusLabel.setFont(juce::FontOptions(12.0f));
    statusLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(keyboard);

    proc.addChangeListener(this);
    refresh();
    startTimerHz(20);
}

HarmoniaEditor::~HarmoniaEditor() {
    stopTimer();
    proc.removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void HarmoniaEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);
    g.setColour(kText);
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.drawText("Harmonia", 16, 8, 200, 30, juce::Justification::centredLeft);
    g.setColour(kMuted);
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(TR("Harmonia determinística, baseada em teoria musical \xE2\x80\x94 sem aleatoriedade nem IA"), 150, 14, 560, 20,
               juce::Justification::centredLeft);
    // legenda do teclado
    g.drawText(TR("\xE2\x97\x8F vermelho = tónica   \xE2\x97\x8F laranja = nota do acorde   verde (1\xE2\x80\x93" "5) = pr\xC3\xB3xima nota recomendada   "
                  "teclas claras = escala   escuras = fora da escala   (clica no teclado para ouvir)"),
               16, 570, getWidth() - 32, 16, juce::Justification::centredLeft);
}

void HarmoniaEditor::resized() {
    auto r = getLocalBounds().reduced(16);
    r.removeFromTop(34);
    statusLabel.setBounds(getWidth() - 416, 12, 400, 20);

    auto row1 = r.removeFromTop(48).withTrimmedTop(18);
    const int gap = 10, w = (row1.getWidth() - gap * 5) / 6;
    for (auto* c : {&keyBox, &scaleBox, &styleBox, &richBox, &spreadBox, &barsBox}) {
        c->setBounds(row1.removeFromLeft(w));
        row1.removeFromLeft(gap);
    }
    r.removeFromTop(6);
    auto row2 = r.removeFromTop(30);
    prevBtn.setBounds(row2.removeFromLeft(110)); row2.removeFromLeft(6);
    genBtn.setBounds(row2.removeFromLeft(80)); row2.removeFromLeft(6);
    nextBtn.setBounds(row2.removeFromLeft(110)); row2.removeFromLeft(16);
    remBtn.setBounds(row2.removeFromLeft(32)); row2.removeFromLeft(4);
    addBtn.setBounds(row2.removeFromLeft(32)); row2.removeFromLeft(16);
    bassTg.setBounds(row2.removeFromLeft(110));
    soundTg.setBounds(row2.removeFromLeft(190));
    dragHandle.setBounds(row2.removeFromRight(230)); row2.removeFromRight(8);
    exportBtn.setBounds(row2.removeFromRight(110));
    r.removeFromTop(4);
    playTg.setBounds(r.removeFromTop(24).removeFromLeft(380));
    r.removeFromTop(6);

    auto slotsRow = r.removeFromTop(112);
    const int cnt = juce::jmax(1, slots.size());
    const int sw = juce::jmin(slotsRow.getWidth() / cnt, 250);
    for (auto* s : slots) s->setBounds(slotsRow.removeFromLeft(sw));
    r.removeFromTop(10);

    auto mid = r.removeFromTop(300 - 16);
    auto left = mid.removeFromLeft(mid.getWidth() / 2 - 6);
    mid.removeFromLeft(12);
    explainBox.setBounds(left);
    auto right = mid;
    auto snapRow = right.removeFromTop(48).withTrimmedTop(18);
    snapBox.setBounds(snapRow);
    right.removeFromTop(6);
    auto btnRow = right.removeFromTop(28);
    const int bw = (btnRow.getWidth() - 3 * 6) / 4;
    analyzeNotesBtn.setBounds(btnRow.removeFromLeft(bw)); btnRow.removeFromLeft(6);
    analyzeProgBtn.setBounds(btnRow.removeFromLeft(bw)); btnRow.removeFromLeft(6);
    useKeyBtn.setBounds(btnRow.removeFromLeft(bw)); btnRow.removeFromLeft(6);
    clearBtn.setBounds(btnRow);
    right.removeFromTop(6);
    reportBox.setBounds(right);

    keyboard.setBounds(getLocalBounds().reduced(16).removeFromBottom(142));
}

void HarmoniaEditor::refresh() {
    auto v = proc.getView();
    if (!v) return;
    const bool minor = proc.isMinorMode();
    const auto tokens = proc.getTokens();
    const auto& palette = chordPalette(minor);
    const int n = (int) v->names.size();

    if (n != slots.size()) {
        slots.clear();
        for (int i = 0; i < n; ++i) {
            auto* s = new ChordSlot(i, [this](int idx) { proc.auditionChord(idx); },
                                    [this](int idx, const std::string& t) { proc.setToken(idx, t); });
            slots.add(s);
            addAndMakeVisible(s);
        }
        resized();
    }
    for (int i = 0; i < n; ++i) {
        juce::String notes;
        for (int m : v->allNotes[(size_t) i]) notes += U8(midiNoteName(m, v->flats)) + " ";
        slots[i]->setContent(U8(v->names[(size_t) i]), notes.trim(), tokens[(size_t) i], palette, i == proc.getFocusChord());
    }

    const auto& st = styles()[(size_t) proc.styleIndex()];
    juce::String text;
    text << TR("ESTILO: ") << TR(st.name) << "\n" << TR(st.description) << "\n\n";
    text << TR("PROGRESSÃO: ");
    for (int i = 0; i < n; ++i) text << U8(v->names[(size_t) i]) << (i + 1 < n ? "  \xE2\x80\x93  " : "");
    text << "\n" << TR("PORQUE FUNCIONA: ") << proc.getWhyText() << "\n\n";
    for (int i = 0; i < n; ++i)
        text << (i + 1) << ". " << U8(v->names[(size_t) i]) << "  [" << U8(v->romans[(size_t) i]) << "]  " << U8(v->functions[(size_t) i]) << "\n";
    text << "\n" << TR("Voice leading: movimento total entre acordes = ") << v->totalMovement << TR(" semitons")
         << (v->voiced.parallelCount == 0 ? TR(" · sem quintas/oitavas paralelas.") : TR(" · atenção: há paralelas."));
    explainBox.setText(text, false);

    statusLabel.setText(TR("Variação ") + juce::String(proc.variationNumber() + 1), juce::dontSendNotification);
}

void HarmoniaEditor::timerCallback() {
    auto v = proc.getView();
    std::array<bool, 128> held{};
    for (int i = 0; i < 128; ++i) held[(size_t) i] = proc.isHeld(i);
    std::array<bool, 12> mask{};
    const Chord* chord = nullptr;
    const int fc = proc.getFocusChord();
    if (v && fc >= 0 && fc < (int) v->chords.size()) {
        chord = &v->chords[(size_t) fc];
        for (int pc : chord->pitchClasses()) mask[(size_t) pc] = true;
    }
    auto sc = proc.getScale();
    keyboard.setInfo(sc, mask, suggestNextNotes(sc, proc.getRecentOutputNotes(6), chord, 5), held);

    const int active = proc.getCurrentChordIndex() >= 0 ? proc.getCurrentChordIndex() : fc;
    if (active != lastActive) { lastActive = active; refresh(); }
}

void HarmoniaEditor::showReport(const Report& r, const juce::String& title) {
    reportBox.clear();
    auto add = [&](const juce::String& s, juce::Colour c) {
        reportBox.setColour(juce::TextEditor::textColourId, c);
        reportBox.insertTextAtCaret(s + "\n");
    };
    add(title + ": " + U8(r.summary), kText);
    if (r.issues.empty() && r.totalNotes > 0) add(TR("\xE2\x9C\x93 Nenhum problema encontrado."), kAccent);
    for (const auto& i : r.issues) {
        juce::Colour c = i.severity == Severity::Clash ? juce::Colour(0xfffc8181) : i.severity == Severity::Warning ? kWarm : kMuted;
        juce::String tag = i.severity == Severity::Clash ? "[Choque] " : i.severity == Severity::Warning ? "[Aviso] " : "[Info] ";
        add("\n" + tag + U8(i.message), c);
        add(TR("   \xE2\x86\x92 ") + U8(i.suggestion), kMuted);
    }
    reportBox.setColour(juce::TextEditor::textColourId, kText);
    reportBox.moveCaretToTop(false);
}

void HarmoniaEditor::analyzeNotes(const std::vector<NoteEvent>& ev, const juce::String& title) {
    auto rep = analyze(ev, proc.getScale());
    detectedKey.reset();
    if (!rep.keyCandidates.empty()) detectedKey = rep.keyCandidates[0];
    useKeyBtn.setEnabled(detectedKey.has_value());
    showReport(rep, title);
}

void HarmoniaEditor::useDetectedKey() {
    if (!detectedKey) return;
    auto setChoice = [&](const char* id, int idx) {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(id))) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1((float) idx));
            p->endChangeGesture();
        }
    };
    setChoice(ids::keyRoot, detectedKey->root);
    setChoice(ids::scaleType, detectedKey->major ? (int) ScaleType::Major : (int) ScaleType::NaturalMinor);
}

bool HarmoniaEditor::isInterestedInFileDrag(const juce::StringArray& files) {
    for (auto& f : files) if (f.endsWithIgnoreCase(".mid") || f.endsWithIgnoreCase(".midi")) return true;
    return false;
}

void HarmoniaEditor::filesDropped(const juce::StringArray& files, int, int) {
    for (auto& path : files) {
        juce::File f(path);
        juce::FileInputStream in(f);
        if (!in.openedOk()) continue;
        juce::MidiFile mf;
        if (!mf.readFrom(in) || mf.getTimeFormat() <= 0) continue;
        const double tpq = mf.getTimeFormat();
        std::vector<NoteEvent> ev;
        for (int t = 0; t < mf.getNumTracks(); ++t) {
            juce::MidiMessageSequence seq(*mf.getTrack(t));
            seq.updateMatchedPairs();
            for (int i = 0; i < seq.getNumEvents(); ++i) {
                auto* e = seq.getEventPointer(i);
                if (!e->message.isNoteOn()) continue;
                double start = e->message.getTimeStamp() / tpq;
                double end = e->noteOffObject ? e->noteOffObject->message.getTimeStamp() / tpq : start + 0.5;
                ev.push_back({e->message.getNoteNumber(), start, juce::jmax(0.05, end - start)});
            }
        }
        analyzeNotes(ev, TR("Ficheiro ") + f.getFileName());
        return;
    }
}
