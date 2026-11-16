import os
import json
import glob
import numpy as np
import soundfile as sf
import librosa
import torch
import faiss
import laion_clap

class SampleLibraryIndexer:
    def __init__(self, index_dir="ai_engine/data"):
        self.index_dir = index_dir
        os.makedirs(self.index_dir, exist_ok=True)
        self.index_path = os.path.join(self.index_dir, "samples.faiss")
        self.meta_path = os.path.join(self.index_dir, "samples_meta.json")
        self.device = "cuda" if torch.cuda.is_available() else "cpu"
        self.model = None
        self.index = None
        self.metadata = []

    def load_clap(self):
        if self.model is None:
            print(f"[CLAP] Caricamento modello audio su {self.device}...")
            self.model = laion_clap.CLAP_Module(enable_fusion=False, device=self.device)
            self.model.load_ckpt()

    def analyze_audio_dsp(self, filepath):
        try:
            y, sr = librosa.load(filepath, sr=22050, duration=8.0)
            duration = librosa.get_duration(y=y, sr=sr)
            tempo, _ = librosa.beat.beat_track(y=y, sr=sr)
            bpm = float(np.round(tempo[0] if isinstance(tempo, np.ndarray) else tempo, 1))
            rms = float(np.mean(librosa.feature.rms(y=y)))
            centroid = float(np.mean(librosa.feature.spectral_centroid(y=y, sr=sr)))
            sample_type = "one_shot" if duration < 2.2 else "loop"
            return {
                "duration_sec": round(duration, 2),
                "bpm": bpm,
                "rms": round(rms, 4),
                "brightness_hz": round(centroid, 1),
                "type": sample_type
            }
        except Exception as e:
            return {"error": str(e), "type": "unknown", "bpm": 120.0}

    def index_folder(self, samples_folder):
        self.load_clap()
        extensions = ("*.wav", "*.aiff", "*.flac", "*.mp3")
        files = []
        for ext in extensions:
            files.extend(glob.glob(os.path.join(samples_folder, "**", ext), recursive=True))

        if not files:
            print(f"[WARNING] Nessun file audio trovato in {samples_folder}")
            return

        print(f"[CLAP] Indicizzazione di {len(files)} sample...")
        embeddings = []
        self.metadata = []

        for idx, fpath in enumerate(files):
            try:
                emb = self.model.get_audio_embedding_from_filelist(x=[fpath], use_tensor=False)
                dsp_info = self.analyze_audio_dsp(fpath)
                embeddings.append(emb[0])
                self.metadata.append({
                    "id": idx,
                    "path": os.path.abspath(fpath),
                    "filename": os.path.basename(fpath),
                    **dsp_info
                })
                if (idx + 1) % 25 == 0:
                    print(f"  -> Elaborati {idx + 1}/{len(files)} file")
            except Exception as e:
                print(f"  [Skip] {fpath}: {e}")

        emb_matrix = np.array(embeddings).astype("float32")
        faiss.normalize_L2(emb_matrix)
        self.index = faiss.IndexFlatIP(emb_matrix.shape[1])
        self.index.add(emb_matrix)

        faiss.write_index(self.index, self.index_path)
        with open(self.meta_path, "w", encoding="utf-8") as f:
            json.dump(self.metadata, f, indent=2)
        print(f"[OK] Indice salvato: {len(self.metadata)} sample pronti per LFM/Orchestratore.")

    def search_samples(self, query_text, top_k=3, sample_type=None):
        self.load_clap()
        if self.index is None and os.path.exists(self.index_path):
            self.index = faiss.read_index(self.index_path)
            with open(self.meta_path, "r", encoding="utf-8") as f:
                self.metadata = json.load(f)

        if self.index is None or not self.metadata:
            return []

        text_emb = self.model.get_text_embedding([query_text], use_tensor=False).astype("float32")
        faiss.normalize_L2(text_emb)
        scores, indices = self.index.search(text_emb, min(top_k * 3, len(self.metadata)))

        results = []
        for score, idx in zip(scores[0], indices[0]):
            if idx < 0:
                continue
            item = self.metadata[idx].copy()
            item["similarity"] = round(float(score), 3)
            if sample_type and item.get("type") != sample_type:
                continue
            results.append(item)
            if len(results) >= top_k:
                break
        return results

if __name__ == "__main__":
    import sys
    folder = sys.argv[1] if len(sys.argv) > 1 else "./samples"
    indexer = SampleLibraryIndexer()
    indexer.index_folder(folder)
