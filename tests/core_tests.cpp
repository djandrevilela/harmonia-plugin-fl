#include <algorithm>
#include <cstdio>
#include <set>

#include "HarmonyChecker.h"
#include "NextNote.h"
#include "Progressions.h"
#include "Theory.h"
#include "VoiceLeading.h"

using namespace harmonia;

static int g_fail = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) { std::printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_fail; } \
    } while (0)

static std::string names(const std::vector<Chord>& cs, bool flats = false) {
    std::string r;
    for (auto& c : cs) r += c.name(flats) + " ";
    return r;
}

int main() {
    // escalas
    Scale cMaj{0, ScaleType::Major};
    CHECK(cMaj.contains(60) && cMaj.contains(64) && !cMaj.contains(61));
    Scale aMin{9, ScaleType::NaturalMinor};
    CHECK(aMin.contains(72) && !aMin.contains(68));
    CHECK(keyPrefersFlats(5, true) && !keyPrefersFlats(7, true));

    // tokens
    auto vi = parseToken("vi");
    CHECK(vi && vi->offset == 9 && vi->quality == Quality::Minor);
    auto b7 = parseToken("bVII");
    CHECK(b7 && b7->offset == 10 && b7->quality == Quality::Major);
    auto v = parseToken("V");
    CHECK(v && v->offset == 7 && v->quality == Quality::Dom);
    auto vii = parseToken("viidim");
    CHECK(vii && vii->offset == 11 && vii->quality == Quality::Dim);
    auto iii = parseToken("III");
    CHECK(iii && iii->quality == Quality::Dom);
    CHECK(!parseToken("xyz"));

    // realização
    auto prog = parseProgression("I V vi IV");
    CHECK(prog.size() == 4);
    CHECK(names(realize(prog, 0, Richness::Add9)) == "Cadd9 Gadd9 Am(add9) Fadd9 ");
    CHECK(names(realize(prog, 0, Richness::Seventh)) == "Cmaj7 G7 Am7 Fmaj7 ");
    CHECK(names(realize(prog, 0, Richness::Triad)) == "C G Am F ");
    CHECK(names(realize(parseProgression("i bVI bIII bVII"), 9, Richness::Triad)) == "Am F C G ");

    // identificação de acordes
    auto id = identifyChord({60, 64, 67});
    CHECK(id && id->name(false) == "C");
    auto id2 = identifyChord({57, 60, 64, 67});  // A C E G: baixo A => Am7
    CHECK(id2 && id2->name(false) == "Am7");
    CHECK(!identifyChord({60, 61}));

    // todos os modelos de todos os estilos são válidos e estão na paleta
    for (const auto& st : styles())
        for (int minor = 0; minor < 2; ++minor) {
            const auto& tpls = minor ? st.minor : st.major;
            CHECK(tpls.size() >= 4);
            const auto& pal = chordPalette(minor);
            for (const auto& t : tpls) {
                auto steps = parseProgression(t.tokens);
                CHECK(steps.size() == 4);
                for (auto& s : steps) CHECK(std::find(pal.begin(), pal.end(), s.token) != pal.end());
            }
        }

    // voice leading: sem paralelas, movimentos curtos, classes de altura corretas
    for (auto rich : {Richness::Triad, Richness::Add9, Richness::Seventh, Richness::Ninth})
        for (const auto& st : styles())
            for (int minor = 0; minor < 2; ++minor)
                for (const auto& t : (minor ? st.minor : st.major)) {
                    auto chords = realize(parseProgression(t.tokens), minor ? 9 : 0, rich);
                    auto vp = voiceLead(chords);
                    CHECK(vp.chords.size() == chords.size());
                    for (size_t i = 0; i < chords.size(); ++i) {
                        std::set<int> got, want;
                        for (int n : vp.chords[i]) { got.insert(mod12(n)); CHECK(n >= 48 && n <= 90); }
                        for (int pc : chords[i].pitchClasses()) want.insert(pc);
                        CHECK(got == want);
                        CHECK(mod12(vp.bass[i]) == chords[i].root && vp.bass[i] >= 36 && vp.bass[i] < 48);
                    }
                    CHECK(vp.parallelCount == 0);
                }
    {
        auto vp = voiceLead(realize(parseProgression("I V vi IV"), 0, Richness::Triad));
        CHECK(vp.totalMovement <= 24);
        auto vp2 = voiceLead(realize(parseProgression("I V vi IV"), 0, Richness::Triad));
        CHECK(vp.chords == vp2.chords);  // determinismo
    }

    // snap
    CHECK(snapToScale(cMaj, 61, SnapMode::Nearest, 60) == 62);  // a subir -> D
    CHECK(snapToScale(cMaj, 61, SnapMode::Nearest, 64) == 60);  // a descer -> C
    CHECK(snapToScale(cMaj, 61, SnapMode::Down) == 60);
    CHECK(snapToScale(cMaj, 61, SnapMode::Up) == 62);
    CHECK(snapToScale(cMaj, 64, SnapMode::Nearest) == 64);

    // próxima nota
    {
        auto s = suggestNextNotes(cMaj, {67, 71}, nullptr, 5);
        CHECK(!s.empty() && s[0].midi == 72);  // sensível resolve na tónica
        Chord cc;
        cc.root = 0;
        auto s2 = suggestNextNotes(cMaj, {64}, &cc, 3);
        for (auto& x : s2) CHECK(x.midi != 65);  // F é nota a evitar sobre C maior com E
        auto s3 = suggestNextNotes(cMaj, {}, nullptr, 3);
        CHECK(s3.size() == 3 && s3[0].midi == 60);
        CHECK(suggestNextNotes(cMaj, {67, 71}, nullptr, 5)[0].reason == suggestNextNotes(cMaj, {67, 71}, nullptr, 5)[0].reason);
    }

    // verificador
    {
        std::vector<NoteEvent> ev = {{60, 0, 4}, {61, 0, 4}};
        auto r = analyze(ev, cMaj);
        bool clash = false, outs = false;
        for (auto& i : r.issues) {
            if (i.severity == Severity::Clash) clash = true;
            if (i.message.find("fora da escala") != std::string::npos) outs = true;
        }
        CHECK(clash && outs && r.outOfScale == 1);

        std::vector<NoteEvent> good = {{60, 0, 4}, {64, 0, 4}, {67, 0, 4}};  // C maior: sem avisos
        CHECK(analyze(good, cMaj).issues.empty());

        std::vector<NoteEvent> key = {{60, 0, 4}, {64, 4, 2}, {67, 6, 2}, {62, 8, 1}, {65, 9, 1}, {69, 10, 1}, {71, 11, 1}, {60, 12, 4}};
        auto kg = estimateKey(key);
        CHECK(!kg.empty() && kg[0].root == 0 && kg[0].major);

        // maj7 reconhecido não é falso alarme de sétima maior
        std::vector<NoteEvent> maj7 = {{48, 0, 4}, {64, 0, 4}, {67, 0, 4}, {71, 0, 4}};
        for (auto& i : analyze(maj7, cMaj).issues) CHECK(i.message.find("Sétima maior") == std::string::npos);

        // sensível em menor natural não é erro grave
        std::vector<NoteEvent> lt = {{68, 0, 1}};
        auto rl = analyze(lt, aMin);
        CHECK(!rl.issues.empty() && rl.issues[0].severity == Severity::Info);
    }

    if (g_fail == 0) std::printf("Todos os testes passaram.\n");
    return g_fail == 0 ? 0 : 1;
}
