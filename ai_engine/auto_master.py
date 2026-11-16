import numpy as np
import soundfile as sf
from pedalboard import Pedalboard, Compressor, HighpassFilter, Limiter, Gain, HighShelfFilter, LowShelfFilter

class NeuralMixMasterEngine:
    """
    Calcola i parametri ottimali per il plugin VST3 e renderizza catene offline
    usando Pedalboard (Spotify DSP).
    """
    def compute_dsp_profile(self, genre: str, target_lufs: float, punch_intent: float):
        genre_lower = genre.lower()

        # Profili calibrati su genere e target loudness
        if "techno" in genre_lower or "rave" in genre_lower or "club" in genre_lower:
            lowcut_hz = 28.0
            punch = max(0.55, punch_intent)
            width = 1.12
            drive_db = 2.4 if target_lufs > -9.0 else 1.5
            ceiling_db = -0.3
        elif "deep" in genre_lower or "ambient" in genre_lower or "house" in genre_lower:
            lowcut_hz = 25.0
            punch = max(0.35, punch_intent)
            width = 1.24
            drive_db = 1.2
            ceiling_db = -0.5
        else:
            lowcut_hz = 30.0
            punch = punch_intent
            width = 1.08
            drive_db = 1.0
            ceiling_db = -0.3

        return {
            "lowcut": float(lowcut_hz),
            "punch": float(np.clip(punch, 0.0, 1.0)),
            "width": float(np.clip(width, 0.5, 1.8)),
            "drive": float(np.clip(drive_db, -6.0, 9.0)),
            "ceiling": float(ceiling_db)
        }

    def render_offline_master(self, input_wav: str, output_wav: str, dsp_params: dict):
        audio, sr = sf.read(input_wav)
        board = Pedalboard([
            Gain(gain_db=dsp_params["drive"]),
            HighpassFilter(cutoff_frequency_hz=dsp_params["lowcut"]),
            LowShelfFilter(cutoff_frequency_hz=90.0, gain_db=1.5),
            Compressor(threshold_db=-16.0 * dsp_params["punch"], ratio=3.5, attack_ms=10.0, release_ms=85.0),
            HighShelfFilter(cutoff_frequency_hz=9500.0, gain_db=1.2),
            Limiter(threshold_db=dsp_params["ceiling"], release_ms=50.0)
        ])
        processed = board(audio, sr)
        sf.write(output_wav, processed, sr)
        return output_wav
