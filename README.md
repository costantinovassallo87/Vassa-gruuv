# Vassa Gruuv

Groove box VST3 per Mac Apple Silicon, ispirata a NI Maschine ma semplificata. C++17 / JUCE 8.

## Funzioni (v0.1)
- **16 pad** (4×4, pad 1 in basso a sinistra) con motore di batteria sintetico integrato (kick, snare, clap, hat con choke, tom, cowbell, crash, shaker, perc) oppure **sample** (drag & drop sul pad, wav/aif/flac/mp3/ogg)
- Per pad: level, pan, tune, decay, mute
- **Sequencer 16 step** (1 battuta, 16 lane, velocity per step, swing), sincronizzato al transport del DAW; in standalone / DAW fermo usa il clock interno (PLAY)
- Input MIDI: note C1–D#2 (36–51) suonano i pad
- Master: filtro low-pass, reverb, volume
- Stato (pattern, parametri, sample) salvato nel progetto del DAW
- **AI Groove**
  - *Generate / Mutate*: generatore locale istantaneo (House, Techno, Hip-Hop, Trap, D&B, Funk), senza rete
  - *Ask Claude*: descrivi un beat a parole ("techno cupa 128 bpm con hat in levare") e Claude restituisce pattern, BPM e swing via API Anthropic. Può anche modificare il pattern corrente ("più ghost note sulla snare"). Chiave API in `API key...` (salvata in `~/Library/Application Support/VassaGruuv`) oppure variabile `ANTHROPIC_API_KEY`

## Build (macOS, Apple Silicon)
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DVG_COPY_PLUGIN=ON
cmake --build build --target VassaGruuv_VST3 -j
```
`VG_COPY_PLUGIN=ON` copia il plugin in `~/Library/Audio/Plug-Ins/VST3`. JUCE viene scaricato automaticamente (o `-DVG_JUCE_DIR=<path>`).
Test logica motore/AI: `cmake --build build --target VassaGruuvTests && ctest --test-dir build`.

La CI (`.github/workflows/build.yml`) compila su macOS arm64, esegue i test e pubblica il `.vst3` come artifact.

## Struttura
- `Source/Engine` – pattern condiviso (lock-free) e voci di batteria
- `Source/AI` – generatore locale e client Claude
- `Source/PluginProcessor.*` – sequencer, mixer, parametri, stato
- `Source/PluginEditor.*` – UI

## Roadmap
Più pattern/scene e song mode · uscite separate per pad · lunghezza pattern e step probability · editor sample (trim/slice) · AI on-device (nessuna API) · Keychain per la chiave · firma/notarizzazione per la distribuzione.
