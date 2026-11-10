#include "HarmonyChecker.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

#include "NextNote.h"
#include "VoiceLeading.h"

namespace harmonia {

namespace {
double pearson(const double* x, const double* y) {
    double mx = 0, my = 0;
    for (int i = 0; i < 12; ++i) { mx += x[i]; my += y[i]; }
    mx /= 12; my /= 12;
    double sxy = 0, sxx = 0, syy = 0;
    for (int i = 0; i < 12; ++i) {
        sxy += (x[i] - mx) * (y[i] - my);
        sxx += (x[i] - mx) * (x[i] - mx);
        syy += (y[i] - my) * (y[i] - my);
    }
    return (sxx > 0 && syy > 0) ? sxy / std::sqrt(sxx * syy) : 0.0;
}
}  // namespace

std::vector<KeyGuess> estimateKey(const std::vector<NoteEvent>& notes) {
    static const double kMaj[12] = {6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88};
    static const double kMin[12] = {6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17};
    double h[12] = {0};
    double total = 0;
    for (const auto& n : notes) {
        double w = std::max(n.length, 0.25);
        h[mod12(n.midi)] += w;
        total += w;
    }
    std::vector<KeyGuess> all;
    if (total <= 0) return all;
    for (int tonic = 0; tonic < 12; ++tonic)
        for (int mode = 0; mode < 2; ++mode) {
            double prof[12];
            for (int pc = 0; pc < 12; ++pc) prof[pc] = (mode == 0 ? kMaj : kMin)[mod12(pc - tonic)];
            all.push_back({tonic, mode == 0, pearson(h, prof)});
        }
    std::sort(all.begin(), all.end(), [](const KeyGuess& a, const KeyGuess& b) {
        if (a.correlation != b.correlation) return a.correlation > b.correlation;
        if (a.root != b.root) return a.root < b.root;
        return a.major && !b.major;
    });
    all.resize(3);
    return all;
}

std::string positionText(double beat) {
    char buf[64];
    int bar = (int) std::floor(beat / 4.0) + 1;
    double b = std::fmod(beat, 4.0) + 1.0;
    std::snprintf(buf, sizeof buf, "compasso %d, tempo %.1f", bar, b);
    return buf;
}

Report analyze(const std::vector<NoteEvent>& notes, const Scale& scale) {
    Report r;
    r.totalNotes = (int) notes.size();
    if (notes.empty()) {
        r.summary = "Sem notas para analisar.";
        return r;
    }
    const bool flats = keyPrefersFlats(scale.root, isMajorFamily(scale.type));
    const std::string keyName = noteName(scale.root, flats) + " " + scaleName(scale.type);
    r.keyCandidates = estimateKey(notes);

    // 1) notas fora da escala
    for (const auto& n : notes) {
        if (scale.contains(n.midi)) continue;
        ++r.outOfScale;
        int pc = mod12(n.midi);
        Issue is;
        is.beat = n.start;
        std::string nm = midiNoteName(n.midi, flats);
        if (scale.type == ScaleType::NaturalMinor && pc == mod12(scale.root + 11)) {
            is.severity = Severity::Info;
            is.message = "A nota " + nm + " é a sensível (7.º grau elevado) (" + positionText(n.start) + ").";
            is.suggestion = "Válida em harmonia menor: cria mais tensão para resolver na tónica. Se preferires o som modal, usa " +
                            midiNoteName(n.midi - 1, flats) + ".";
        } else {
            is.severity = n.length < 0.5 ? Severity::Info : Severity::Warning;
            is.message = "A nota " + nm + " está fora da escala de " + keyName + " (" + positionText(n.start) + ").";
            is.suggestion = "Troca por " + midiNoteName(snapToScale(scale, n.midi, SnapMode::Nearest, -1), flats) +
                            " (a mais próxima na escala)" + (n.length < 0.5 ? " ou mantém como nota de passagem." : ".");
        }
        r.issues.push_back(is);
    }

    // 2) choques e intervalos graves em notas simultâneas
    std::vector<double> b;
    for (const auto& n : notes) { b.push_back(n.start); b.push_back(n.start + n.length); }
    std::sort(b.begin(), b.end());
    b.erase(std::unique(b.begin(), b.end(), [](double x, double y) { return std::abs(x - y) < 1e-6; }), b.end());
    std::set<std::pair<int, int>> clashFlagged, lowFlagged;
    for (size_t k = 0; k + 1 < b.size(); ++k) {
        double t0 = b[k], t1 = b[k + 1];
        if (t1 - t0 < 1e-6) continue;
        std::vector<int> act;
        for (size_t i = 0; i < notes.size(); ++i)
            if (notes[i].start < t1 - 1e-6 && notes[i].start + notes[i].length > t0 + 1e-6) act.push_back((int) i);
        if (act.size() < 2) continue;
        std::stable_sort(act.begin(), act.end(), [&](int x, int y) { return notes[(size_t) x].midi < notes[(size_t) y].midi; });
        std::vector<int> midis;
        for (int i : act) midis.push_back(notes[(size_t) i].midi);
        bool isChord = identifyChord(midis).has_value();
        if (!isChord)
            for (size_t x = 0; x < act.size(); ++x)
                for (size_t y = x + 1; y < act.size(); ++y) {
                    int ai = std::abs(midis[y] - midis[x]);
                    if (ai != 1 && ai != 13 && ai != 11) continue;
                    auto key = std::make_pair(act[x], act[y]);
                    if (!clashFlagged.insert(key).second) continue;
                    Issue is;
                    is.beat = t0;
                    std::string a = midiNoteName(midis[x], flats), c = midiNoteName(midis[y], flats);
                    if (ai == 11) {
                        is.severity = Severity::Warning;
                        is.message = "Sétima maior entre " + a + " e " + c + " fora de um acorde reconhecido (" + positionText(t0) + ").";
                        is.suggestion = "Se não for intencional, move uma das notas; se for um maj7, completa o acorde com terça e quinta.";
                    } else {
                        is.severity = Severity::Clash;
                        is.message = "Semitono entre " + a + " e " + c + " a soar em simultâneo (" + positionText(t0) + ").";
                        is.suggestion = "Afasta uma das notas pelo menos um tom, ou muda-a de oitava (a nona maior soa muito melhor).";
                    }
                    r.issues.push_back(is);
                }
        for (size_t x = 0; x + 1 < act.size(); ++x) {
            int lo = midis[x], gap = midis[x + 1] - lo;
            if (gap <= 0 || gap >= 5 || lo >= 48) continue;
            if (!lowFlagged.insert({act[x], act[x + 1]}).second) continue;
            Issue is;
            is.severity = Severity::Info;
            is.beat = t0;
            is.message = "Intervalo pequeno (" + std::to_string(gap) + " semitons) em registo grave, a partir de " +
                         midiNoteName(lo, flats) + " (" + positionText(t0) + "): pode ficar enlameado.";
            is.suggestion = "Abre o intervalo para uma quinta ou oitava, ou sobe uma das notas.";
            r.issues.push_back(is);
        }
    }

    // 3) quintas/oitavas paralelas entre ataques consecutivos
    std::vector<int> order(notes.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = (int) i;
    std::stable_sort(order.begin(), order.end(), [&](int x, int y) { return notes[(size_t) x].start < notes[(size_t) y].start; });
    std::vector<std::vector<int>> groups;
    std::vector<double> gStart;
    for (int i : order) {
        const auto& n = notes[(size_t) i];
        if (groups.empty() || n.start - gStart.back() > 0.06) { groups.emplace_back(); gStart.push_back(n.start); }
        groups.back().push_back(n.midi);
    }
    for (auto& g : groups) std::sort(g.begin(), g.end());
    for (size_t g = 1; g < groups.size(); ++g)
        if (groups[g - 1].size() >= 3 && groups[g].size() == groups[g - 1].size() && hasParallelPerfects(groups[g - 1], groups[g])) {
            Issue is;
            is.severity = Severity::Info;
            is.beat = gStart[g];
            is.message = "Quintas ou oitavas paralelas entre os acordes de " + positionText(gStart[g - 1]) + " e " + positionText(gStart[g]) + ".";
            is.suggestion = "Pode soar vazio. Em EDM é comum (efeito \"power chord\"); se queres mais suavidade, usa o Voice Leading automático.";
            r.issues.push_back(is);
        }

    std::stable_sort(r.issues.begin(), r.issues.end(), [](const Issue& a, const Issue& b) {
        if (std::abs(a.beat - b.beat) > 1e-9) return a.beat < b.beat;
        return (int) a.severity > (int) b.severity;
    });

    int pct = (int) std::lround(100.0 * (r.totalNotes - r.outOfScale) / r.totalNotes);
    r.summary = std::to_string(pct) + "% das notas estão dentro da escala (" + keyName + ").";
    if (!r.keyCandidates.empty()) {
        const auto& k = r.keyCandidates[0];
        r.summary += " Tom mais provável: " + noteName(k.root, keyPrefersFlats(k.root, k.major)) + (k.major ? " maior" : " menor") + ".";
        Scale guess{k.root, k.major ? ScaleType::Major : ScaleType::NaturalMinor};
        if (guess.pitchClassMask() != scale.pitchClassMask()) r.summary += " (Diferente da escala escolhida.)";
    }
    return r;
}

}  // namespace harmonia
