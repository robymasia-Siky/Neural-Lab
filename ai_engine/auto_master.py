import os
import numpy as np
import soundfile as sf
import librosa
import matchering as mg
from pedalboard import Pedalboard, Compressor, HighpassFilter, Limiter, Gain, HighShelfFilter, LowShelfFilter

class NeuralMixMasterEngine:
    """
    Analizza realmente l'audio degli stems selezionati (conflitti sub/low-mid,
    energia RMS, crest factor) per calcolare la catena ottimale sul VST3
    e applica Matchering 2.0 per il mastering finale.
    """
    def analyze_stems_interaction(self, stem_paths: list) -> dict:
        if not stem_paths:
            return {"sub_conflict": 0.5, "avg_crest_db": 10.0, "brightness": 2500.0}

        sub_energies = []
        crest_factors = []
        centroids = []

        for path in stem_paths:
            if not os.path.exists(path):
                continue
            y, sr = librosa.load(path, sr=22050, duration=6.0)
            if len(y) == 0:
                continue

            # Analisi energia sub (20-90 Hz) per rilevare mascheramento Kick/Bass
            S = np.abs(librosa.stft(y))
            freqs = librosa.fft_frequencies(sr=sr)
            sub_mask = (freqs >= 20) & (freqs <= 90)
            sub_energy = float(np.mean(S[sub_mask, :])) if np.any(sub_mask) else 0.0
            sub_energies.append(sub_energy)

            # Crest Factor (Peak / RMS) per decidere quanta compressione serve
            rms = float(np.sqrt(np.mean(y**2))) + 1e-8
            peak = float(np.max(np.abs(y))) + 1e-8
            crest_db = 20.0 * np.log10(peak / rms)
            crest_factors.append(crest_db)

            centroids.append(float(np.mean(librosa.feature.spectral_centroid(S=S, sr=sr))))

        return {
            "sub_conflict": float(np.sum(sub_energies)) if sub_energies else 0.5,
            "avg_crest_db": float(np.mean(crest_factors)) if crest_factors else 10.0,
            "brightness": float(np.mean(centroids)) if centroids else 2500.0
        }

    def compute_dsp_profile(self, genre: str, target_lufs: float, punch_intent: float, stem_paths: list = None):
        acoustic = self.analyze_stems_interaction(stem_paths or [])
        genre_lower = genre.lower()

        # 1. Sub Clean (High-Pass) adattivo in base all'accumulo di sub reali
        lowcut_hz = 26.0
        if acoustic["sub_conflict"] > 1.8:
            lowcut_hz = 34.0  # Pulisce fangosita' se kick e basso spingono troppo insieme

        # 2. Punch adattivo sul Crest Factor reale dei sample
        crest = acoustic["avg_crest_db"]
        auto_punch = np.clip((crest - 6.0) / 12.0, 0.2, 0.85)
        punch = float(0.5 * punch_intent + 0.5 * auto_punch)

        # 3. Stereo Width in base al genere
        width = 1.18 if ("melodic" in genre_lower or "deep" in genre_lower) else 1.08

        # 4. Input Drive calibrato verso il Target LUFS
        drive_db = np.clip((-14.0 - target_lufs) * 0.45, 0.0, 6.0)
        ceiling_db = -0.3

        return {
            "lowcut": float(round(lowcut_hz, 1)),
            "punch": float(round(punch, 2)),
            "width": float(round(width, 2)),
            "drive": float(round(drive_db, 1)),
            "ceiling": float(ceiling_db)
        }

    def master_with_reference(self, target_wav: str, reference_wav: str, output_wav: str):
        """Applica Matchering 2.0 (EQ, RMS, Stereo Width, Limiter) usando una reference track."""
        mg.process(
            target=target_wav,
            reference=reference_wav,
            results=[mg.pcm24(output_wav)]
        )
        return output_wav
