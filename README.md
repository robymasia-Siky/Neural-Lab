# NEURAL LAB — AI Studio Producer & Auto-Mastering Suite

![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=c%2B%2B) ![JUCE 8](https://img.shields.io/badge/JUCE-8.0.4-8CC63F?style=for-the-badge) ![Python 3.11](https://img.shields.io/badge/Python-3.11-3776AB?style=for-the-badge&logo=python&logoColor=white) ![PyTorch CUDA](https://img.shields.io/badge/PyTorch-CUDA%2012.4-EE4C2C?style=for-the-badge&logo=pytorch&logoColor=white) ![VST3](https://img.shields.io/badge/Format-VST3%20%7C%20Standalone-00E5FF?style=for-the-badge) ![License: MIT](https://img.shields.io/badge/License-MIT-F59E0B?style=for-the-badge)

**NEURAL LAB** is a 100% local AI-powered music production, sample curation, and auto-mastering suite pairing a **C++ / JUCE VST3 & Standalone plugin** (*Dark Hardware* UI) with a **GPU-accelerated Python AI & DSP engine**.

## Key Features
- **Dark Hardware VST3/Standalone Console:** Integrated prompt bar (`GENERATE TRACK` & `AUTO MIX & MASTER`), 64-band 60fps FFT spectrum analyzer, real-time **GR / LUFS / OUT** meters, and live AI Stems inspector.
- **Multi-Folder Sample Libraries & Reference Mastering:** Index unlimited local sample folders (`+ SAMPLE FOLDER`) via **LAION-CLAP + FAISS** and lock onto commercial reference tracks (`LOAD REFERENCE`) via **Matchering 2.0**.
- **Real-Time C++ DSP Engine:** Input Drive, Sub Clean High-Pass, Neural Punch Compressor, Mid/Side Stereo Width, and True-Peak Mastering Limiter.
- **Local 0.5B Micro-Orchestrator + Ableton Live Sync:** Uses local Ollama LLM (`<50ms` latency) and **AbletonOSC** to set BPM, create audio tracks, load matched stems into clips, and trigger playback.

## Architecture
```text
[NEURAL LAB VST3/Standalone] <--OSC 9001/9002--> [Python GPU Engine: 0.5B LLM + CLAP + Spectral DSP] --OSC 11000--> [Ableton Live]
```

## Quick Start
1. **Python GPU Setup:** `py -3.11 -m venv .venv && .\.venv\Scripts\pip install -r ai_engine/requirements.txt && ollama pull qwen2.5:0.5b`
2. **Build VST3/Standalone:** `cd plugin && cmake -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Release -j 8`
3. **Run:** Launch **NEURAL LAB** (Standalone or in Ableton); the AI backend starts automatically in the background.

## License
Distributed under the **MIT License**. See [`LICENSE`](LICENSE) for details.
