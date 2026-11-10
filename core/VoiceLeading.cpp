#include "VoiceLeading.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace harmonia {

namespace {

struct Cand {
    std::vector<int> notes;
    double unary = 0;
};

void gen(const std::vector<std::vector<int>>& pos, size_t i, std::vector<int>& cur, int maxSpan,
         std::vector<std::vector<int>>& out) {
    if (i == pos.size()) {
        auto v = cur;
        std::sort(v.begin(), v.end());
        if (v.back() - v.front() <= maxSpan) out.push_back(v);
        return;
    }
    for (int m : pos[i]) {
        cur.push_back(m);
        gen(pos, i + 1, cur, maxSpan, out);
        cur.pop_back();
    }
}

double unaryCost(const std::vector<int>& v) {
    double avg = 0;
    for (int n : v) avg += n;
    avg /= (double) v.size();
    double u = std::abs(avg - 62.0) * 0.3;
    if (v.back() > 74) u += (v.back() - 74) * 0.6;
    for (size_t i = 1; i < v.size(); ++i) {
        int gap = v[i] - v[i - 1];
        if (gap == 1) u += 4.0;       // semitono entre vozes adjacentes: áspero
        else if (gap == 2) u += 0.5;
    }
    if (v.size() >= 2 && v[0] < 55 && v[1] - v[0] < 3) u += 2.0;  // grave enlameado
    return u;
}

double movement(const std::vector<int>& a, const std::vector<int>& b) {
    size_t n = std::min(a.size(), b.size());
    double m = 0;
    for (size_t k = 0; k < n; ++k) m += std::abs(a[a.size() - 1 - k] - b[b.size() - 1 - k]);  // alinhado pelo topo
    m += 3.0 * std::abs((int) a.size() - (int) b.size());
    return m;
}

}  // namespace

bool hasParallelPerfects(const std::vector<int>& a, const std::vector<int>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        for (size_t j = i + 1; j < a.size(); ++j) {
            int ia = mod12(a[j] - a[i]), ib = mod12(b[j] - b[i]);
            if (ia != ib || (ia != 7 && ia != 0)) continue;
            int di = b[i] - a[i], dj = b[j] - a[j];
            if (di != 0 && dj != 0 && ((di > 0) == (dj > 0))) return true;
        }
    return false;
}

VoicedProgression voiceLead(const std::vector<Chord>& chords, const VoicingOptions& opt) {
    VoicedProgression res;
    if (chords.empty()) return res;

    std::vector<std::vector<Cand>> cands(chords.size());
    for (size_t c = 0; c < chords.size(); ++c) {
        auto pcs = chords[c].pitchClasses();
        int span = opt.maxSpan;
        for (int attempt = 0; attempt < 4 && cands[c].empty(); ++attempt, span += 5) {
            std::vector<std::vector<int>> pos(pcs.size());
            for (size_t k = 0; k < pcs.size(); ++k)
                for (int m = opt.low; m <= opt.high; ++m)
                    if (mod12(m) == pcs[k]) pos[k].push_back(m);
            std::vector<std::vector<int>> raw;
            std::vector<int> cur;
            gen(pos, 0, cur, span, raw);
            for (auto& v : raw) cands[c].push_back({v, unaryCost(v)});
        }
    }

    const double kInf = std::numeric_limits<double>::infinity();
    std::vector<std::vector<double>> dp(chords.size());
    std::vector<std::vector<int>> prev(chords.size());
    for (size_t c = 0; c < chords.size(); ++c) {
        dp[c].assign(cands[c].size(), kInf);
        prev[c].assign(cands[c].size(), -1);
    }
    for (size_t k = 0; k < cands[0].size(); ++k) dp[0][k] = cands[0][k].unary;
    for (size_t c = 1; c < chords.size(); ++c)
        for (size_t k = 0; k < cands[c].size(); ++k)
            for (size_t j = 0; j < cands[c - 1].size(); ++j) {
                const auto& A = cands[c - 1][j].notes;
                const auto& B = cands[c][k].notes;
                double cost = dp[c - 1][j] + cands[c][k].unary + movement(A, B);
                if (hasParallelPerfects(A, B)) cost += 20.0;
                double leap = std::abs(A.back() - B.back());
                if (leap > 4) cost += (leap - 4) * 0.8;
                if (cost < dp[c][k]) { dp[c][k] = cost; prev[c][k] = (int) j; }
            }

    size_t last = chords.size() - 1;
    int best = 0;
    for (size_t k = 0; k < dp[last].size(); ++k)
        if (dp[last][k] < dp[last][(size_t) best]) best = (int) k;
    std::vector<int> choice(chords.size());
    for (size_t c = chords.size(); c-- > 0;) {
        choice[c] = best;
        best = prev[c][(size_t) best];
    }

    for (size_t c = 0; c < chords.size(); ++c) {
        res.chords.push_back(cands[c][(size_t) choice[c]].notes);
        res.bass.push_back(opt.includeBass ? opt.bassLow + mod12(chords[c].root - opt.bassLow) : -1);
        if (c > 0) {
            res.totalMovement += (int) movement(res.chords[c - 1], res.chords[c]);
            if (hasParallelPerfects(res.chords[c - 1], res.chords[c])) ++res.parallelCount;
        }
    }
    return res;
}

}  // namespace harmonia
