import os
import re
import numpy as np
import soundfile as sf
from scipy import signal

VAR_OUTPUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "variations")
os.makedirs(VAR_OUTPUT_DIR, exist_ok=True)

def _pitch_shift_preserve_len(audio: np.ndarray, semitones: float) -> np.ndarray:
    if abs(semitones) < 0.01 or len(audio) < 64:
        return audio
    ratio = 2.0 ** (semitones / 12.0)
    orig_len = len(audio)
    shifted_len = max(32, int(orig_len / ratio))
    if audio.ndim == 1:
        resampled = signal.resample(audio, shifted_len)
        x_old = np.linspace(0.0, 1.0, len(resampled))
        x_new = np.linspace(0.0, 1.0, orig_len)
        return np.interp(x_new, x_old, resampled).astype(np.float32)
    else:
        out = np.zeros((orig_len, audio.shape[1]), dtype=np.float32)
        for ch in range(audio.shape[1]):
            resampled = signal.resample(audio[:, ch], shifted_len)
            x_old = np.linspace(0.0, 1.0, len(resampled))
            x_new = np.linspace(0.0, 1.0, orig_len)
            out[:, ch] = np.interp(x_new, x_old, resampled)
        return out

def create_audio_variation(filepath: str, role: str, var_index: int = 1, target_bpm: float = 126.0) -> tuple:
    """
    Genera 3 varianti creative dal sample originale:
      1 = Harmonic Fifth (+5 semitones, o +7 per synth/lead)
      2 = Dark Tape Saturation & Low-Pass Filter
      3 = Transient Punch & Stereo Air Boost
    Ritorna (output_path, display_name, description)
    """
    if not filepath or not os.path.isfile(filepath):
        raise FileNotFoundError(f"File sorgente non trovato: {filepath}")

    data, sr = sf.read(filepath, dtype="float32", always_2d=True)
    max_frames = int(sr * 30)
    if len(data) > max_frames:
        data = data[:max_frames]

    mode = ((var_index - 1) % 3) + 1
    base_name = os.path.splitext(os.path.basename(filepath))[0]
    base_name = re.sub(r"_VAR\d+.*$", "", base_name)

    if mode == 1:
        semi = 7.0 if any(k in role.upper() for k in ["LEAD", "SYNTH", "ARP", "PAD"]) else 5.0
        proc = _pitch_shift_preserve_len(data, semi)
        tag = f"VAR1_Pitch+{int(semi)}st"
        desc = f"Harmonic Pitch +{int(semi)}st"
    elif mode == 2:
        driven = np.tanh(data * 2.2) / np.tanh(2.2)
        sos = signal.butter(2, min(2800.0, sr * 0.45), btype="low", fs=sr, output="sos")
        proc = signal.sosfilt(sos, driven, axis=0).astype(np.float32)
        tag = "VAR2_DarkTape"
        desc = "Dark Tape Saturation & LP Filter"
    else:
        sos_hi = signal.butter(2, min(4200.0, sr * 0.45), btype="high", fs=sr, output="sos")
        highs = signal.sosfilt(sos_hi, data, axis=0)
        proc = np.tanh((data + highs * 0.65) * 1.35) / np.tanh(1.35)
        proc = proc.astype(np.float32)
        tag = "VAR3_PunchAir"
        desc = "Transient Punch & Stereo Air"

    peak = float(np.max(np.abs(proc))) if len(proc) > 0 else 0.0
    if peak > 1e-6:
        proc = proc * (0.944 / peak)

    out_filename = f"{base_name}_{tag}.wav"
    out_path = os.path.join(VAR_OUTPUT_DIR, out_filename)
    sf.write(out_path, proc, sr, subtype="PCM_24")
    return out_path, out_filename, desc
