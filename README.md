# Vassa Gruuv

Groove box VST3 per Mac Apple Silicon, ispirata a NI Maschine ma semplificata. C++17 / JUCE 8.

## Layout (dall'alto al basso)
1. **Step sequencer** – 16 tracce × 16 step, velocity per step, mute/solo per traccia, swing. Cliccare una traccia la seleziona.
2. **Modulation** – lane per step della traccia selezionata: *Velocity, Length, Pitch, Repeat (ratchet 1–8), Nudge (−/+ 50% di step), Chance, Slice*. Trascina per disegnare le barre, doppio click / Alt = reset, rotella = fine. Gli step con modulazioni mostrano un puntino azzurro.
3. **Sampler** (stile Ableton Simpler) – segue la traccia selezionata:
   - modi **Classic** (ADSR + loop con crossfade), **One-Shot**, **Slice** (divisioni uguali o rilevamento transienti con sensibilità)
   - waveform con marker trascinabili (start/end/loop), fade in/out, reverse, drag & drop di file audio
   - transpose, detune, gain, pan; filtro LP/HP/BP; LFO (pitch/filter/volume/pan)
   - **inviluppi ADSR** per Amp, Pitch e Filter (Pitch e Filter con un *Amount* in semitoni), con selettore per vederne uno alla volta. L'ADSR si modifica **direttamente sulla waveform** trascinando i nodi (A, D/S, R; il rilascio parte a fine step) e con i knob a lato; tutti i valori sono parametri del plugin, quindi **automatizzabili dal DAW**. La waveform è *adattiva*: mostra in tempo reale ciò che si sente (campione × inviluppo × fade), con la forma originale come fantasma. In One-Shot/Slice il rilascio non si usa: l'inviluppo tiene il livello di sustain fino a fine sample
   - waveform con zoom (rotella; Shift+rotella = scorri; doppio click = reset), righello temporale, marker S/E/L con lettura in ms durante il drag
   - ogni traccia parte con un suono di fabbrica sintetizzato (pulsante *Factory* per ripristinarlo)

## GUI
Tema scuro stile hardware, colore per famiglia di tracce, knob con arco (bipolare dove serve) e valore sempre visibile, codice colore per sezione (output/amp arancio, filtro ciano, LFO viola). La finestra è ridimensionabile (proporzioni fisse, 75–150%).

## Routing / export
- **16 uscite stereo separate** (*Track 1…16*) + *Main Mix*. In un host VST3 abilita le uscite che vuoi: una traccia con la propria uscita attiva non finisce più nel Main Mix. Filtro/reverb/master agiscono solo sul Main Mix.
- **Drag & drop multitraccia**: trascina il badge *DRAG STEMS TO DAW* su una traccia del DAW → un WAV 24-bit stereo per ogni traccia con step (1/2/4/8 battute, coda del reverb ripiegata all'inizio per loop senza buchi). *Export…* scrive gli stessi file in una cartella.
- Input MIDI: note C1–D#2 (36–51) suonano le tracce.

## Pattern tools / AI (backend già presente, UI da integrare)
*Generate / Mutate* (generatore locale per stile) e client Claude (`requestClaude`, chiave API da `ANTHROPIC_API_KEY` o impostazioni utente) sono in `Source/AI` e `PluginProcessor`; nella UI restano solo Generate/Mutate/Clear.

## Build (macOS, Apple Silicon)
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DVG_COPY_PLUGIN=ON
cmake --build build --target VassaGruuv_VST3 -j
```
`VG_COPY_PLUGIN=ON` copia il plugin in `~/Library/Audio/Plug-Ins/VST3`. JUCE viene scaricato automaticamente (o `-DVG_JUCE_DIR=<path>`).
Test: `cmake --build build --target VassaGruuvTests VassaGruuvProcessorTest && ctest --test-dir build`.
La CI (`.github/workflows/build.yml`) compila su macOS arm64, lancia i test e pubblica il `.vst3`.

## Struttura
- `Source/Engine` – `Pattern` (lane per step, lock-free), `Scheduler` (step → trigger con repeat/nudge/chance/swing), `Sampler` (voci Simpler-style), `Exporter` (stems offline), `Drums`/`Factory` (suoni di fabbrica)
- `Source/AI` – generatore locale e client Claude
- `Source/PluginProcessor.*` – parametri, routing, stato · `Source/PluginEditor.*` – UI
