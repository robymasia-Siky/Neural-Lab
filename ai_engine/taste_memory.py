import os
import json
from datetime import datetime

class StudioTasteMemory:
    def __init__(self, memory_path: str):
        self.memory_path = memory_path
        os.makedirs(os.path.dirname(self.memory_path), exist_ok=True)

    def record_decision(self, genre: str, ref_track: str, dsp_values: dict, note: str = ""):
        entry = {
            "timestamp": datetime.now().isoformat(timespec="seconds"),
            "genre": genre or "electronic",
            "reference": os.path.basename(ref_track) if ref_track else "None",
            "dsp": dsp_values,
            "note": note
        }
        try:
            with open(self.memory_path, "a", encoding="utf-8") as f:
                f.write(json.dumps(entry, ensure_ascii=False) + "\n")
        except Exception:
            pass

    def get_recent_taste_summary(self, limit: int = 3) -> str:
        if not os.path.isfile(self.memory_path):
            return ""
        try:
            with open(self.memory_path, "r", encoding="utf-8") as f:
                lines = [l.strip() for l in f if l.strip()]
            if not lines:
                return ""
            recent = [json.loads(l) for l in lines[-limit:]]
            parts = []
            for r in recent:
                d = r.get("dsp", {})
                parts.append(
                    f"{r.get('genre')} (Sub={d.get('lowcut', 30)}Hz, Punch={d.get('punch', 0.4)}, Width={d.get('width', 1.1)}, Drive={d.get('drive', 1.0)}dB)"
                )
            return "PREFERENZE STORICHE DEL PRODUCER: " + " | ".join(parts)
        except Exception:
            return ""
