# Harmonia — princípios do projeto (ler sempre antes de mexer no código)

Plugin VST3 (JUCE) para FL Studio que ajuda produtores de EDM sem muita teoria musical.

## Regras inegociáveis (definidas pelo autor)
1. **Nada de geração aleatória nem IA.** Todas as sugestões/gerações são determinísticas e derivadas de
   regras de teoria musical (harmonia funcional, condução de vozes, escalas, tendências melódicas).
   Mesmas entradas => mesmas saídas. Proibido `rand`, `std::random_*`, modelos ML, etc.
2. **Só acordes/notas que funcionam** musicalmente (dentro da escala/função harmónica, ou empréstimos modais conhecidos).
3. **Sem infringir copyright.** Não copiar melodias, progressões "assinatura" de músicas específicas, nem usar
   nomes de artistas/músicas no produto. Só padrões harmónicos genéricos (não protegíveis) e regras de teoria.
4. Interface e textos em português europeu (pt-PT).
5. O motor (`core/`) é C++17 puro, sem JUCE, e tem testes (`tests/`). A UI/áudio (`plugin/`) só orquestra.

## Estrutura
- `core/Theory`        escalas, acordes, nomes, identificação de acordes
- `core/Progressions`  estilos e modelos de progressão (tokens de numerais romanos)
- `core/VoiceLeading`  condução de vozes por programação dinâmica (sem quintas/oitavas paralelas)
- `core/NextNote`      scale lock/correção + recomendação de próxima nota
- `core/HarmonyChecker` deteção de tom, notas fora da escala, choques, intervalos graves, paralelas
- `plugin/`            JUCE: processador (MIDI in/out, sintetizador de pré-escuta), editor, export MIDI
