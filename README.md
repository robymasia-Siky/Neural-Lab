# NEURAL LAB — Dark Hardware AI Studio & Auto-Mastering Suite

Sistema ibrido locale composto da un plugin **VST3/Standalone (C++ / JUCE)** con estetica *Dark Hardware* e un motore **AI & DSP (Python)** ottimizzato per GPU consumer (8GB VRAM).

## Architettura
- **`plugin/` (C++ / JUCE):** Interfaccia Dark Hardware (All-in-One), analizzatore spettrale, catena DSP real-time (EQ, Compressore, Limiter) e bridge OSC/HTTP a bassa latenza.
- **`ai_engine/` (Python / GPU):**
  - *Orchestratore:* Micro-LLM locale (LFM / Qwen) per regia, arrangiamento e routing JSON.
  - *Audio Intelligence:* LAION-CLAP per ricerca semantica nella libreria sample + motore neurale di Auto-Mix & Mastering.
- **`training/`:** Script per dataset sintetici e fine-tuning QLoRA.
