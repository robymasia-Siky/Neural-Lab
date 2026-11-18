<p align='center'><img src='assets/neural_lab_logo.svg' alt='NEURAL-LAB Logo' width='840'/></p>

<h1 align='center'>NEURAL-LAB — Next-Gen AI Music Production Studio &amp; DSP Engine</h1>

<p align='center'><strong>Ideato, progettato e sviluppato da <a href='https://github.com/robymasia-Siky'>Robymasia-Siky</a></strong><br/><em>Plugin VST3 &amp; Standalone con interfaccia <strong>Obsidian Glass v2.0</strong> che unisce DSP audio professionale in tempo reale (C++20/JUCE con 4x Oversampling e Club Mono-Maker), Intelligenza Artificiale 100% locale (Ollama + LAION-CLAP), Drag &amp; Drop diretto verso DAW, Reference A/B e controllo di Ableton Live tramite MCP.</em></p>

<p align='center'><img src='https://img.shields.io/badge/Ideatore-Robymasia--Siky-00e5ff?style=for-the-badge'/> <img src='https://img.shields.io/badge/UI-Obsidian%20Glass%20v2.0-00e5ff?style=for-the-badge'/> <img src='https://img.shields.io/badge/DSP-4x%20Oversampling%20%2B%20Mono--Maker-f59e0b?style=for-the-badge'/> <img src='https://img.shields.io/badge/Format-VST3%20%7C%20Standalone-38bdf8?style=for-the-badge'/> <img src='https://img.shields.io/badge/DAW-Ableton%20Live%20MCP-10b981?style=for-the-badge'/></p>

---

## 🎯 Perché NEURAL-LAB
- **Zero Cloud, Zero Abbonamenti, 100% Locale:** Tutti i modelli AI girano sul tuo PC tramite **Ollama** (`neural-lab-0.5b` + `qwen2.5-coder:3b`) e **LAION-CLAP**. I tuoi progetti inediti restano sempre privati.
- **Motore DSP Club-Grade (C++20 / JUCE):** Catena audio a latenza zero con **4x Polyphase Oversampling**, **Analog Soft-Clipper**, **Club Mono-Maker (< 120 Hz)** sul canale Side, **8-Band Auto-EQ Matchering** e **True-Peak Brickwall Limiter**.
- **Workflow Diretto in Studio (Drag & Drop + One-Click DSP):** Trascina qualsiasi stem Audio o MIDI dalla UI direttamente sulle tracce di Ableton Live e applica con un clic i profili DSP suggeriti dall'AI Copilot.

## 🚀 Funzionalità Principali
| Modulo | Descrizione | Stack Tecnico |
| :--- | :--- | :--- |
| **⚡ Console Mastering & DSP 3x2** | **4x Oversampling**, saturatore armonico `tanh`, **Club Mono-Maker (< 120 Hz)**, 8-Band Auto-EQ Match e True-Peak Limiter. | C++20 • JUCE DSP • Polyphase IIR |
| **💬 AI Copilot + One-Click DSP Apply** | Sidebar verticale a tutta altezza con chat scrollabile, quick-actions (**GENERATE / ARRANGE / MASTER**) e pulsante **APPLY DSP** contestuale. | Ollama • neural-lab-0.5b • JUCE APVTS |
| **🧲 Drag & Drop Stems verso DAW** | Trascinamento nativo di campioni Audio e clip MIDI generate verso Ableton Live + controlli **Audition (`>`) / Lock / Swap**. | JUCE DragAndDropContainer • SMF MIDI |
| **📊 Spectrum Analyzer Continuo & A/B** | Curva spettrale fluida **Cyan Live** con glow + **Amber Reference Target** sovrapposta (`> REF` / `|| REF`) e meter slim **GR / LUFS / OUT**. | JUCE Path Spline • FFT • AudioTransportSource |
| **🎛️ Ableton Live MCP Bridge** | Controllo diretto di tracce MIDI, note, BPM, rinomina canali e volumi su Ableton Live (`MCP: ON/OFF`). | AbletonOSC • qwen2.5-coder:3b • UDP 11000 |

## 💻 Avvio Rapido
1. **Standalone:** Avvia `NEURAL LAB.exe` (il motore AI e Ollama si avviano automaticamente su `NEURAL-LAB ENGINE: ONLINE`).
2. **Plugin VST3:** Copia `NEURAL LAB.vst3` in `C:\Program Files\Common Files\VST3\` e caricalo sul Master o su un bus di Ableton Live.

## 📖 Documentazione & Crediti
- Consulta il **[Manuale d'Uso Completo (MANUALE_USO.md)](MANUALE_USO.md)** per tutti i dettagli operativi.
- **Ideatore e Autore del Progetto:** **[Robymasia-Siky](https://github.com/robymasia-Siky)**
