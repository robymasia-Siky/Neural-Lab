# NEURAL-LAB — Manuale d'Uso Completo (Edizione 2026)

## 1. Installazione e Primo Avvio
1. Copia `NAURAL LAB.vst3`in `C:\Program Files\Common Files\VST3\` eh avvia la scansione plugin nella DAW.
2. Apri NEURAL LAB: il server Python (`ai_engine/server.py`) si avvia automaticamente in background fino allo stato `NEURAL-LAB ENGINE: ONLINE`.
3. Clicca `+ FOLDER` per indicizzare le tue cartelle sample locali tramite CLAP.

## 2. Mastering & Dynamic DOP
- **INPUT DRIVE (-12 / +12 dB):** Guadagno verso saturazione e compressione.
- **SUB CLEAN (20-300 Hz):** Filtro passa-alto lineare.
- **NEURAL PUNCH (0-100%):** Compressione dinamica adattiva + saturazione armonica 4x oversampled.
- **STEREO WIDTH (0-200%):** Controllo Mid/Side con Club Mono-Maker (< 120 Hz in mono).
- **EQ MATCH (0-100%):** Applica l'EQ Match a 8 bande sulla Reference Track e attiva il **Dynamic Spectral De-Mud (210 Hz & 340 Hz)** fino a -4.5 dB.
- **CEILING (-6.0 / 0.0 dB):** Soglia del True-Peak Limiter 4x oversampled.
- **Auto-Gain Reference A/B (> REF / || MATCH):** Compensa automaticamente la differenza di volume LUFS tra il tuo mix e la Reference Track.
- **Metering (LUFS | dBTP | PLR):** Loudness percepito, picco reale inter-sample e dinamica residua.

## 3. Stems AI, Sample Variations (VAR) e Drag & Drop
- `>`: Ascolta in anteprima lo stem.
- `LOCK`: Blocca lo stem scielto nelle generazioni successive.
- `VAR`: Genera 3 varianti pronte per il Drag & Drop: VAR #1 Harmonic Pitch (+5/+7st), VAR #2 Dark Tape Saturation & LP Filter, VAR #3 Transient Punch & Stereo Air (oppure variazioni MIDI in tonalità).
- `SWAP`: Sostituisce lo stem con l'alternativa successiva.
- **Drag & Drop:** Trascina lo stem direttamente dal pannello AI STEMS nella timeline di Ableton Live.

## 4. Copilot & Memoria di Gusto (Phase 3)
- Chiedi analisi di mix/mastering al Copilot (`neural-lab-0.5b`) e clicca `APPLY DSP` per applicare i parametri consigliati.
- Ogni click salva la preferenza in `ai_engine/memory/engineer_log.jsonl`. Usa `python training/distill_memory.py` per aggiungere i tuoi gusti al dataset QLoRA.

## 5. Controllo Ableton Live via MCP (qwen2.5-coder:3b)
Attiva `MCP: ON` in alto per comandare Ableton in linguaggio naturale: creazione tracce/clip MIDI, Arrangement View, Macro 1..8, curve Filter Sweep (`exp_up`, `s_curve`), Send A/B e Mute/Solo.
