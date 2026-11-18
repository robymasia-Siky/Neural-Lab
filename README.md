<p align='center'><img src='assets/neural_lab_logo.svg' alt='NEURAL-LAB Logo' width='840'/></p>

<h1 align='center'>NEURAL-LAB — Next-Gen AI Music Production Studio &amp; DSP Engine</h1>

<p align='center'><strong>Ideato, progettato e sviluppato da <a href='https://github.com/robymasia-Siky'>Robymasia-Siky</a></strong><br/><em>Plugin VST3 &amp; Standalone con interfaccia <strong>Obsidian Glass v2.0</strong> che unisce DSP audio professionale in tempo reale (C++20/JUCE), Intelligenza Artificiale 100% locale (Ollama + LAION-CLAP), Reference A/B, Auto-Mastering adattivo e controllo diretto di Ableton Live tramite MCP.</em></p>

<p align='center'><img src='https://img.shields.io/badge/Ideatore-Robymasia--Siky-00e5ff?style=for-the-badge'/> <img src='https://img.shields.io/badge/UI-Obsidian%20Glass%20v2.0-00e5ff?style=for-the-badge'/> <img src='https://img.shields.io/badge/Format-VST3%20%7C%20Standalone-38bdf8?style=for-the-badge'/> <img src='https://img.shields.io/badge/AI-100%25%20Local%20Ollama%20%2B%20CLAP-f59e0b?style=for-the-badge'/> <img src='https://img.shields.io/badge/DAW-Ableton%20Live%20MCP-10b981?style=for-the-badge'/></p>

---

## 🎯 Perché NEURAL-LAB
- **Zero Cloud, Zero Abbonamenti, 100% Locale:** Tutti i modelli AI girano sul tuo PC tramite **Ollama** e **PyTorch/ONNX**. I tuoi progetti inediti restano sempre privati.
- **Latenza Audio Zero:** Motore DSP nativo in **C++20 / JUCE** separato dal backend neurale Python tramite bus **OSC UDP (9001/9002)** con avvio istantaneo (< 0.5s).
- **Obsidian Glass Two-Column Studio (v2.0):** Interfaccia widescreen a due colonne con **Spectrum Analyzer a curva continua** (Cyan Live + Amber Reference Target) e rack modulari Stems/DSP a sinistra, affiancati dalla **AI Copilot Sidebar** verticale a tutta altezza sulla destra.

## 🚀 Funzionalità Principali
| Modulo | Descrizione | Stack Tecnico |
| :--- | :--- | :--- |
| **📊 Spectrum Analyzer Continuo & A/B** | Curva spettrale fluida **Cyan Live** con glow + **Amber Reference Target** sovrapposta (`> REF` / `|| REF`) e meter slim **GR / LUFS / OUT**. | JUCE Path Spline • FFT • AudioTransportSource |
| **💬 AI Copilot Sidebar (Full-Height)** | Pannello verticale dedicato a destra con chat scrollabile, input prompt e quick-action pills (**GENERATE**, **ARRANGE**, **MASTER**). | Ollama • neural-lab-0.5b • JUCE |
| **🎛️ Ableton Live MCP Bridge** | Controllo diretto di tracce MIDI, note, BPM, rinomina canali e volumi su Ableton Live (`MCP: ON/OFF`). | AbletonOSC • qwen2.5-coder:3b • UDP 11000 |
| **🔍 Sample Indexer Semantico** | Scansione cartelle (`+ SAMPLES`) con selezione intelligente di Kick, Bass, Synth, Perc + **Audition (`>`) / Lock / Swap**. | LAION-CLAP • PyTorch |
| **⚡ Console Mastering & DSP 3x2** | Griglia modulare con **INPUT DRIVE**, **SUB CLEAN**, **NEURAL PUNCH**, **STEREO WIDTH**, **EQ MATCH %** e **CEILING**. | C++20 DSP • Mid/Side • Matchering |

## 💻 Avvio Rapido
1. **Standalone:** Avvia `NEURAL LAB.exe` (il motore AI e Ollama si avviano automaticamente su `NEURAL-LAB ENGINE: ONLINE`).
2. **Plugin VST3:** Copia `NEURAL LAB.vst3` in `C:\Program Files\Common Files\VST3\` e caricalo sul Master o su un bus di Ableton Live.

## 📖 Documentazione & Crediti
- Consulta il **[Manuale d'Uso Completo (MANUALE_USO.md)](MANUALE_USO.md)** per tutti i dettagli operativi.
- **Ideatore e Autore del Progetto:** **[Robymasia-Siky](https://github.com/robymasia-Siky)**
