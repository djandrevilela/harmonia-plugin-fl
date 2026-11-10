#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "Util.h"

using namespace harmonia;

namespace {
const char* kKeyNames[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
const double kBarsValues[4] = {0.5, 1.0, 2.0, 4.0};
}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout HarmoniaAudioProcessor::createLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    StringArray keys, scales, stys;
    for (auto* k : kKeyNames) keys.add(k);
    for (int i = 0; i < kNumScaleTypes; ++i) scales.add(TR(scaleName((ScaleType) i)));
    for (auto& s : styles()) stys.add(TR(s.name));

    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ids::keyRoot, 1}, TR("Tom"), keys, 9));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ids::scaleType, 1}, TR("Escala"), scales, 1));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ids::style, 1}, TR("Estilo"), stys, 0));
    l.add(std::make_unique<AudioParameterChoice>(
        ParameterID{ids::richness, 1}, TR("Riqueza"),
        StringArray{TR("Automático (do estilo)"), TR("Tríades"), "Add9", TR("Sétimas"), "Nonas"}, 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ids::spread, 1}, TR("Voicing"),
                                                 StringArray{"Fechado", "Aberto"}, 0));
    l.add(std::make_unique<AudioParameterChoice>(
        ParameterID{ids::bars, 1}, TR("Duração por acorde"),
        StringArray{TR("1/2 compasso"), TR("1 compasso"), TR("2 compassos"), TR("4 compassos")}, 1));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{ids::bass, 1}, TR("Baixo (raiz)"), true));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{ids::playProg, 1}, TR("Tocar progressão"), false));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{ids::sound, 1}, TR("Som interno"), true));
    l.add(std::make_unique<AudioParameterChoice>(
        ParameterID{ids::snap, 1}, TR("Scale lock"),
        StringArray{"Desligado", TR("Escala (mais próxima)"), TR("Escala (para baixo)"), TR("Escala (para cima)"),
                    TR("Notas do acorde")},
        1));
    return l;
}

HarmoniaAudioProcessor::HarmoniaAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "HARMONIA", createLayout()) {
    for (auto& a : held) a.store(false);
    for (auto& row : mapped) row.fill(-1);
    rec.reserve(2048);
    progNotes.reserve(32);
    audNotes.reserve(32);
    synth.addSound(new HarmSound());
    for (int i = 0; i < 16; ++i) synth.addVoice(new HarmVoice());

    for (auto* id : {ids::keyRoot, ids::scaleType, ids::style, ids::richness, ids::spread, ids::bars, ids::bass})
        apvts.addParameterListener(id, this);

    lastMinor = isMinorMode();
    loadTemplate();
    rebuild();
}

HarmoniaAudioProcessor::~HarmoniaAudioProcessor() {
    cancelPendingUpdate();
    for (auto* id : {ids::keyRoot, ids::scaleType, ids::style, ids::richness, ids::spread, ids::bars, ids::bass})
        apvts.removeParameterListener(id, this);
}

// ------------------------------------------------------------ parâmetros / modelo
Scale HarmoniaAudioProcessor::getScale() const {
    Scale s;
    s.root = juce::jlimit(0, 11, (int) apvts.getRawParameterValue(ids::keyRoot)->load());
    s.type = (ScaleType) juce::jlimit(0, kNumScaleTypes - 1, (int) apvts.getRawParameterValue(ids::scaleType)->load());
    return s;
}
bool HarmoniaAudioProcessor::isMinorMode() const { return !isMajorFamily(getScale().type); }
int HarmoniaAudioProcessor::styleIndex() const {
    return juce::jlimit(0, (int) styles().size() - 1, (int) apvts.getRawParameterValue(ids::style)->load());
}
Richness HarmoniaAudioProcessor::effectiveRichness() const {
    switch ((int) apvts.getRawParameterValue(ids::richness)->load()) {
        case 1: return Richness::Triad;
        case 2: return Richness::Add9;
        case 3: return Richness::Seventh;
        case 4: return Richness::Ninth;
        default: return styles()[(size_t) styleIndex()].defaultRichness;
    }
}

void HarmoniaAudioProcessor::parameterChanged(const juce::String& id, float) {
    if (id == ids::style) repick = true;
    triggerAsyncUpdate();
}

void HarmoniaAudioProcessor::handleAsyncUpdate() {
    bool minorNow = isMinorMode();
    if (repick.exchange(false) || minorNow != lastMinor) {
        lastMinor = minorNow;
        loadTemplate();
    }
    rebuild();
}

void HarmoniaAudioProcessor::loadTemplate() {
    const auto& st = styles()[(size_t) styleIndex()];
    const auto& tpls = isMinorMode() ? st.minor : st.major;
    int n = (int) tpls.size();
    variation = ((variation % n) + n) % n;
    juce::ScopedLock sl(modelLock);
    steps = parseProgression(tpls[(size_t) variation].tokens);
    whyText = tpls[(size_t) variation].why;
}

void HarmoniaAudioProcessor::generate(int delta) {
    variation += delta;
    loadTemplate();
    rebuild();
}

std::vector<std::string> HarmoniaAudioProcessor::getTokens() const {
    juce::ScopedLock sl(modelLock);
    std::vector<std::string> t;
    for (auto& s : steps) t.push_back(s.token);
    return t;
}

void HarmoniaAudioProcessor::setToken(int slot, const std::string& token) {
    auto p = parseToken(token);
    if (!p) return;
    {
        juce::ScopedLock sl(modelLock);
        if (slot < 0 || slot >= (int) steps.size()) return;
        steps[(size_t) slot] = *p;
        whyText = "Progressão editada por ti: o Voice Leading foi recalculado para manter as transições suaves.";
    }
    rebuild();
}

void HarmoniaAudioProcessor::addSlot() {
    {
        juce::ScopedLock sl(modelLock);
        if (steps.size() >= 8) return;
        auto p = parseToken(isMinorMode() ? "i" : "I");
        steps.push_back(*p);
    }
    rebuild();
}

void HarmoniaAudioProcessor::removeSlot() {
    {
        juce::ScopedLock sl(modelLock);
        if (steps.size() <= 1) return;
        steps.pop_back();
    }
    rebuild();
}

juce::String HarmoniaAudioProcessor::getWhyText() const {
    juce::ScopedLock sl(modelLock);
    return U8(whyText);
}

void HarmoniaAudioProcessor::rebuild() {
    std::vector<ChordStep> local;
    {
        juce::ScopedLock sl(modelLock);
        local = steps;
    }
    auto d = std::make_shared<PlaybackData>();
    const Scale sc = getScale();
    d->flats = keyPrefersFlats(sc.root, isMajorFamily(sc.type));
    d->chords = realize(local, sc.root, effectiveRichness());

    VoicingOptions vo;
    const bool open = (int) apvts.getRawParameterValue(ids::spread)->load() == 1;
    vo.maxSpan = open ? 21 : 14;
    vo.high = open ? 79 : 76;
    vo.includeBass = apvts.getRawParameterValue(ids::bass)->load() > 0.5f;
    d->voiced = voiceLead(d->chords, vo);
    d->totalMovement = d->voiced.totalMovement;
    d->barsPerChord = kBarsValues[juce::jlimit(0, 3, (int) apvts.getRawParameterValue(ids::bars)->load())];

    for (size_t i = 0; i < local.size(); ++i) {
        d->names.push_back(d->chords[i].name(d->flats));
        d->romans.push_back(prettyToken(local[i].token));
        d->functions.push_back(functionText(local[i]));
        std::vector<int> all;
        if (d->voiced.bass[i] >= 0) all.push_back(d->voiced.bass[i]);
        for (int n : d->voiced.chords[i]) all.push_back(n);
        d->allNotes.push_back(all);
    }
    {
        juce::SpinLock::ScopedLockType sl(dataLock);
        graveyard.push_back(published);
        if (graveyard.size() > 4) graveyard.erase(graveyard.begin());
        published = d;
    }
    sendChangeMessage();
}

std::shared_ptr<const PlaybackData> HarmoniaAudioProcessor::getView() const {
    juce::SpinLock::ScopedLockType sl(dataLock);
    return published;
}

void HarmoniaAudioProcessor::auditionChord(int index) {
    focusChord = index;
    auditionRequest = index;
}

// ------------------------------------------------------------ áudio
void HarmoniaAudioProcessor::prepareToPlay(double sr, int) {
    synth.setCurrentPlaybackSampleRate(sr);
    internalBeat = 0.0;
    progWasPlaying = false;
    lastAbsChord = -1;
}

int HarmoniaAudioProcessor::snapNote(int note, int mode, const Scale& sc) const {
    if (mode == 0) return note;
    if (mode == 4) {
        int fc = focusChord.load();
        if (audioData && fc >= 0 && fc < (int) audioData->chords.size()) {
            std::array<bool, 12> mask{};
            for (int pc : audioData->chords[(size_t) fc].pitchClasses()) mask[(size_t) pc] = true;
            return snapToPitchClasses(mask, note, SnapMode::Nearest, lastInputNote);
        }
        return snapToScale(sc, note, SnapMode::Nearest, lastInputNote);
    }
    SnapMode m = mode == 2 ? SnapMode::Down : mode == 3 ? SnapMode::Up : SnapMode::Nearest;
    return snapToScale(sc, note, m, lastInputNote);
}

void HarmoniaAudioProcessor::releaseProgNotes(juce::MidiBuffer& out, int offset) {
    for (int n : progNotes) out.addEvent(juce::MidiMessage::noteOff(1, n), offset);
    progNotes.clear();
}
void HarmoniaAudioProcessor::releaseAudition(juce::MidiBuffer& out, int offset) {
    for (int n : audNotes) out.addEvent(juce::MidiMessage::noteOff(1, n), offset);
    audNotes.clear();
}
void HarmoniaAudioProcessor::switchChord(juce::MidiBuffer& out, int offset, int idx) {
    releaseProgNotes(out, offset);
    if (!audioData || idx < 0 || idx >= (int) audioData->allNotes.size()) return;
    for (int n : audioData->allNotes[(size_t) idx]) {
        out.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 92), offset);
        progNotes.push_back(n);
    }
    playingChord = idx;
    focusChord = idx;
}

void HarmoniaAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();
    {
        juce::SpinLock::ScopedTryLockType tl(dataLock);
        if (tl.isLocked()) audioData = published;
    }
    keyboardState.processNextMidiBuffer(midi, 0, n, true);

    bool playing = false, hasPpq = false;
    double ppq = 0.0, bpm = 120.0;
    int num = 4, den = 4;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) { ppq = *p; hasPpq = true; }
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto ts = pos->getTimeSignature()) { num = ts->numerator; den = ts->denominator; }
        }
    const double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
    const double bps = bpm / 60.0 / sr;  // tempos por amostra
    const double recBeat = hasPpq ? ppq : internalBeat;
    internalBeat += n * bps;

    outMidi.clear();
    const Scale scale = getScale();
    const int snapMode = (int) apvts.getRawParameterValue(ids::snap)->load();

    // 1) reprodução da progressão sincronizada com o anfitrião
    const bool progOn = apvts.getRawParameterValue(ids::playProg)->load() > 0.5f;
    if (audioData && !audioData->allNotes.empty()) {
        const int N = (int) audioData->allNotes.size();
        if (progOn && playing && hasPpq) {
            const double beatsPerBar = num * 4.0 / (double) den;
            const double bpc = audioData->barsPerChord * beatsPerBar;
            long long absChord = (long long) std::floor(ppq / bpc + 1e-6);
            if (!progWasPlaying || absChord != lastAbsChord) {
                switchChord(outMidi, 0, (int) (((absChord % N) + N) % N));
                lastAbsChord = absChord;
            }
            double nextB = (double) (absChord + 1) * bpc, endPpq = ppq + n * bps;
            if (nextB < endPpq) {
                int off = juce::jlimit(0, n - 1, (int) ((nextB - ppq) / bps));
                lastAbsChord = absChord + 1;
                switchChord(outMidi, off, (int) (((lastAbsChord % N) + N) % N));
            }
            progWasPlaying = true;
        } else if (progWasPlaying) {
            releaseProgNotes(outMidi, 0);
            playingChord = -1;
            progWasPlaying = false;
        }

        // 2) pré-escuta de um acorde (clique no editor)
        int aud = auditionRequest.exchange(-1);
        if (aud >= 0 && aud < N) {
            releaseAudition(outMidi, 0);
            for (int note : audioData->allNotes[(size_t) aud]) {
                outMidi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) 92), 0);
                audNotes.push_back(note);
            }
            audSamplesLeft = (int) (sr * 1.6);
        }
    }
    if (audSamplesLeft > 0) {
        audSamplesLeft -= n;
        if (audSamplesLeft <= 0) { releaseAudition(outMidi, juce::jmax(0, n - 1)); audSamplesLeft = 0; }
    }

    // 3) MIDI de entrada com scale lock / correção
    for (const auto meta : midi) {
        auto m = meta.getMessage();
        const int pos = meta.samplePosition;
        const int ch = juce::jlimit(1, 16, m.getChannel());
        if (m.isNoteOn()) {
            const int note = m.getNoteNumber();
            const int out = juce::jlimit(0, 127, snapNote(note, snapMode, scale));
            mapped[(size_t) ch - 1][(size_t) note] = out;
            outMidi.addEvent(juce::MidiMessage::noteOn(ch, out, m.getVelocity()), pos);
            {
                juce::SpinLock::ScopedTryLockType tl(recLock);
                if (tl.isLocked()) {
                    if (rec.size() >= 2000) rec.erase(rec.begin(), rec.begin() + 200);
                    double t = recBeat + pos * bps;
                    rec.push_back({note, out, t, t, true});
                }
            }
            lastInputNote = out;
        } else if (m.isNoteOff()) {
            const int note = m.getNoteNumber();
            int out = mapped[(size_t) ch - 1][(size_t) note];
            if (out < 0) out = note;
            mapped[(size_t) ch - 1][(size_t) note] = -1;
            outMidi.addEvent(juce::MidiMessage::noteOff(ch, out, m.getVelocity()), pos);
            juce::SpinLock::ScopedTryLockType tl(recLock);
            if (tl.isLocked()) {
                double t = recBeat + pos * bps;
                for (size_t i = rec.size(); i-- > 0;)
                    if (rec[i].open && rec[i].midi == note) { rec[i].open = false; rec[i].end = t; break; }
            }
        } else {
            outMidi.addEvent(m, pos);
        }
    }

    // estado das teclas para o editor
    for (const auto meta : outMidi) {
        auto m = meta.getMessage();
        if (m.isNoteOn()) held[(size_t) m.getNoteNumber()].store(true);
        else if (m.isNoteOff()) held[(size_t) m.getNoteNumber()].store(false);
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (auto& h : held) h.store(false);
    }

    if (apvts.getRawParameterValue(ids::sound)->load() > 0.5f) synth.renderNextBlock(buffer, outMidi, 0, n);
    midi.swapWith(outMidi);
}

// ------------------------------------------------------------ gravação para o verificador
std::vector<int> HarmoniaAudioProcessor::getRecentOutputNotes(int maxCount) const {
    std::vector<int> r;
    juce::SpinLock::ScopedLockType sl(recLock);
    for (size_t i = rec.size() > (size_t) maxCount ? rec.size() - (size_t) maxCount : 0; i < rec.size(); ++i) r.push_back(rec[i].out);
    return r;
}

std::vector<NoteEvent> HarmoniaAudioProcessor::getRecordedNotes() const {
    std::vector<NoteEvent> r;
    juce::SpinLock::ScopedLockType sl(recLock);
    double lastT = 0;
    for (auto& x : rec) lastT = juce::jmax(lastT, x.start);
    for (auto& x : rec) {
        double end = x.open ? juce::jmax(x.start + 0.5, lastT) : x.end;
        r.push_back({x.midi, x.start, juce::jmax(0.05, end - x.start)});
    }
    return r;
}

void HarmoniaAudioProcessor::clearRecorded() {
    juce::SpinLock::ScopedLockType sl(recLock);
    rec.clear();
}

// ------------------------------------------------------------ MIDI export
juce::String HarmoniaAudioProcessor::suggestedFileName() const {
    auto sc = getScale();
    bool fl = keyPrefersFlats(sc.root, isMajorFamily(sc.type));
    return "Harmonia_" + U8(noteName(sc.root, fl)) + (isMajorFamily(sc.type) ? "maj" : "min") + "_" +
           juce::String(styleIndex() + 1) + "_v" + juce::String(variation + 1);
}

bool HarmoniaAudioProcessor::writeMidiFile(const juce::File& f) const {
    auto d = getView();
    if (!d || d->allNotes.empty()) return false;
    const int tpq = 960;
    const int len = (int) (d->barsPerChord * 4.0 * tpq);
    juce::MidiMessageSequence seq;
    seq.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4));
    seq.addEvent(juce::MidiMessage::tempoMetaEvent(500000));
    for (size_t c = 0; c < d->allNotes.size(); ++c) {
        const double t0 = (double) c * len;
        for (int note : d->allNotes[c]) {
            seq.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) 96), t0);
            seq.addEvent(juce::MidiMessage::noteOff(1, note), t0 + len - 4);
        }
    }
    seq.updateMatchedPairs();
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote(tpq);
    mf.addTrack(seq);
    f.deleteFile();
    juce::FileOutputStream os(f);
    if (!os.openedOk()) return false;
    return mf.writeTo(os);
}

// ------------------------------------------------------------ estado
void HarmoniaAudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto state = apvts.copyState();
    state.setProperty("progression", U8(joinTokens([&] { juce::ScopedLock sl(modelLock); return steps; }())), nullptr);
    state.setProperty("variation", variation, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void HarmoniaAudioProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) {
            auto vt = juce::ValueTree::fromXml(*xml);
            apvts.replaceState(vt);
            variation = (int) vt.getProperty("variation", 0);
            auto parsed = parseProgression(vt.getProperty("progression", "").toString().toStdString());
            repick = false;
            lastMinor = isMinorMode();
            if (parsed.empty()) loadTemplate();
            else {
                juce::ScopedLock sl(modelLock);
                steps = parsed;
            }
            rebuild();
        }
}

juce::AudioProcessorEditor* HarmoniaAudioProcessor::createEditor() { return new HarmoniaEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HarmoniaAudioProcessor(); }
