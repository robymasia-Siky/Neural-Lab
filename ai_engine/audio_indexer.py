import os
import json
import glob
import numpy as np
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
        self._load_existing()

    def _load_existing(self):
        if os.path.exists(self.index_path) and os.path.exists(self.meta_path):
            self.index = faiss.read_index(self.index_path)
            with open(self.meta_path, "r", encoding="utf-8") as f:
                self.metadata = json.load(f)

    def load_clap(self):
        if self.model is None:
            self.model = laion_clap.CLAP_Module(enable_fusion=False, device=self.device)
            self.model.load_ckpt()

    def analyze_audio_dsp(self, filepath):
        try:
            y, sr = librosa.load(filepath, sr=22050, duration=6.0)
            duration = librosa.get_duration(y=y, sr=sr)
            tempo, _ = librosa.beat.beat_track(y=y, sr=sr)
            bpm = float(np.round(tempo[0] if isinstance(tempo, np.ndarray) else tempo, 1))
            sample_type = "one_shot" if duration < 2.2 else "loop"
            return {"duration_sec": round(duration, 2), "bpm": bpm, "type": sample_type}
        except Exception:
            return {"duration_sec": 1.0, "bpm": 126.0, "type": "unknown"}

    def add_folder_incremental(self, samples_folder):
        self.load_clap()
        existing_paths = {item["path"] for item in self.metadata}
        extensions = ("*.wav", "*.aiff", "*.flac", "*.mp3")
        files = []
        for ext in extensions:
            files.extend(glob.glob(os.path.join(samples_folder, "**", ext), recursive=True))

        new_files = [os.path.abspath(f) for f in files if os.path.abspath(f) not in existing_paths]
        if not new_files:
            return len(self.metadata)

        new_embeddings = []
        start_id = len(self.metadata)

        for idx, fpath in enumerate(new_files):
            try:
                emb = self.model.get_audio_embedding_from_filelist(x=[fpath], use_tensor=False)
                dsp_info = self.analyze_audio_dsp(fpath)
                new_embeddings.append(emb[0])
                self.metadata.append({
                    "id": start_id + idx,
                    "path": fpath,
                    "filename": os.path.basename(fpath),
                    **dsp_info
                })
            except Exception:
                continue

        if new_embeddings:
            emb_matrix = np.array(new_embeddings).astype("float32")
            faiss.normalize_L2(emb_matrix)
            if self.index is None:
                self.index = faiss.IndexFlatIP(emb_matrix.shape[1])
            self.index.add(emb_matrix)

            faiss.write_index(self.index, self.index_path)
            with open(self.meta_path, "w", encoding="utf-8") as f:
                json.dump(self.metadata, f, indent=2)

        return len(self.metadata)

    def search_samples(self, query_text, top_k=1, sample_type=None):
        self.load_clap()
        if self.index is None or not self.metadata:
            return []

        text_emb = self.model.get_text_embedding([query_text], use_tensor=False).astype("float32")
        faiss.normalize_L2(text_emb)
        scores, indices = self.index.search(text_emb, min(top_k * 4, len(self.metadata)))

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
