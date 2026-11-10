# Harmonia (VST3) — assistente de harmonia para FL Studio

Plugin para produtores de EDM com pouca teoria musical. Tudo é **determinístico e baseado em teoria**
(sem aleatoriedade, sem IA) e usa apenas padrões harmónicos genéricos (sem copiar músicas).

## O que faz (v0.1)

- **Gerador de progressões por estilo**: Emocional, Pop energético, Épico, Future bass, Progressive house, Build-up.
  Cada estilo tem 4 variações (maior e menor), com explicação em português do *porquê* de funcionar.
  Podes trocar qualquer acorde por outro da paleta (numerais romanos) e o resto recalcula.
- **Voice leading automático**: escolhe as inversões que minimizam o movimento entre acordes e evitam quintas/oitavas paralelas.
- **Scale lock e correção**: o MIDI que tocas é ajustado para a escala (mais próxima/baixo/cima) ou para as notas do acorde.
  Nos empates, a direção do movimento decide.
- **Recomendação de próxima nota**: o teclado mostra as 5 melhores notas seguintes (1–5) com a razão (sensível resolve,
  nota do acorde, evitar semitono, grau conjunto...).
- **Verificador de harmonia**: deteta o tom provável, notas fora da escala, choques de semitono, intervalos graves
  enlameados e paralelas. Analisa as notas que tocaste, a progressão, ou um `.mid` largado sobre o plugin.
- **Exportar/arrastar MIDI** para o piano roll ou playlist do FL Studio.

## Compilar

### Opção A — GitHub Actions (sem instalar nada)
1. Cria um repositório no GitHub e faz push desta pasta (branch `main`).
2. Separador *Actions* → *Build VST3 (Windows)* → quando terminar, descarrega o artefacto `Harmonia-VST3`.

### Opção B — Windows local
Requisitos: Visual Studio 2022 (carga "Desenvolvimento para ambiente de trabalho com C++"), CMake ≥ 3.22, Git.
```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Harmonia_VST3
```
O resultado fica em `build/Harmonia_artefacts/Release/VST3/Harmonia.vst3`.

### Só o motor + testes (qualquer sistema)
```
cmake -B build -DHARMONIA_BUILD_PLUGIN=OFF && cmake --build build && ctest --test-dir build
```

## Usar no FL Studio
1. Copia `Harmonia.vst3` para `C:\Program Files\Common Files\VST3\`.
2. *Options → Manage plugins → Find installed plugins* (Verify plugins).
3. Adiciona **Harmonia** como gerador no Channel rack.
4. **Ouvir/arrastar progressões**: clica num acorde para ouvir; arrasta o botão *Arrastar MIDI* para o piano roll ou playlist.
5. **Scale lock em tempo real**: no ícone de engrenagem do Harmonia (Wrapper) → separador *Processing*/MIDI, define a
   *Output port* (ex.: 1) e põe a mesma porta em *Input port* noutro instrumento. Desliga "Som interno" no Harmonia.
   O MIDI corrigido sai do Harmonia para esse instrumento.
6. **Tocar progressão**: liga "Tocar progressão com o FL"; os acordes seguem o transporte do FL (a mesma saída MIDI).

> Nota: esta versão foi compilada e testada (motor) em Linux. O comportamento dentro do FL Studio (routing MIDI, drag & drop)
> ainda precisa de ser validado na tua máquina — diz-me o que encontrares e ajusto.

## Estrutura
Ver `CLAUDE.md` (princípios) e as pastas `core/` (teoria, testada em `tests/`) e `plugin/` (JUCE).

## Licenças
O JUCE é AGPLv3 ou comercial. Para distribuir o plugin fechado/vender, é preciso uma licença JUCE adequada.
O nome "Harmonia" é provisório: confirma que não colide com marcas existentes antes de lançar.
