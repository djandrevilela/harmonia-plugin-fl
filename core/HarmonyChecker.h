#pragma once
// Verificador de harmonia: deteta tom, notas fora da escala, choques, intervalos graves e paralelas.
#include <string>
#include <vector>

#include "Theory.h"

namespace harmonia {

struct NoteEvent {
    int midi = 60;
    double start = 0;   // em tempos (beats)
    double length = 1;  // em tempos
};

enum class Severity { Info = 0, Warning = 1, Clash = 2 };

struct Issue {
    Severity severity = Severity::Info;
    double beat = 0;
    std::string message;
    std::string suggestion;
};

struct KeyGuess {
    int root = 0;
    bool major = true;
    double correlation = 0;
};

struct Report {
    std::vector<KeyGuess> keyCandidates;  // até 3, melhor primeiro
    std::vector<Issue> issues;
    int totalNotes = 0;
    int outOfScale = 0;
    std::string summary;
};

std::vector<KeyGuess> estimateKey(const std::vector<NoteEvent>& notes);  // Krumhansl–Schmuckler
Report analyze(const std::vector<NoteEvent>& notes, const Scale& scale);
std::string positionText(double beat);  // assume 4/4

}  // namespace harmonia
