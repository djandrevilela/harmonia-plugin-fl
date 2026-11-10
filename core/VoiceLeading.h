#pragma once
// Condução de vozes determinística (programação dinâmica): minimiza movimento, evita quintas/oitavas paralelas.
#include <vector>

#include "Theory.h"

namespace harmonia {

struct VoicingOptions {
    int low = 48;       // nota mais grave permitida na harmonia (C3)
    int high = 76;      // nota mais aguda permitida (E5)
    int maxSpan = 14;   // abertura máxima do acorde (semitons)
    bool includeBass = true;
    int bassLow = 36;   // a raiz do baixo fica em [bassLow, bassLow+11]
};

struct VoicedProgression {
    std::vector<std::vector<int>> chords;  // vozes superiores, ordenadas
    std::vector<int> bass;                 // -1 se não houver baixo
    int totalMovement = 0;                 // soma dos movimentos entre acordes (semitons)
    int parallelCount = 0;                 // transições com quintas/oitavas paralelas
};

VoicedProgression voiceLead(const std::vector<Chord>& chords, const VoicingOptions& opt = {});
bool hasParallelPerfects(const std::vector<int>& a, const std::vector<int>& b);

}  // namespace harmonia
