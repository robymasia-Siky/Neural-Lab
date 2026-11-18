import os
import glob

class NeuralSkillRouter:
    def __init__(self, skills_dir: str):
        self.skills_dir = skills_dir
        self.skills = []
        self.reload_skills()

    def reload_skills(self):
        self.skills = []
        if not os.path.isdir(self.skills_dir):
            return
        for path in sorted(glob.glob(os.path.join(self.skills_dir, "*.md"))):
            parsed = self._parse_skill_file(path)
            if parsed:
                self.skills.append(parsed)

    def _parse_skill_file(self, path: str):
        try:
            with open(path, "r", encoding="utf-8") as f:
                raw = f.read()
            meta = {
                "name": os.path.basename(path),
                "target_model": "neural-lab-0.5b",
                "triggers": [],
                "dsp_preset": None,
                "body": raw.strip(),
            }
            if raw.startswith("---"):
                parts = raw.split("---", 2)
                if len(parts) >= 3:
                    header_lines = parts[1].strip().splitlines()
                    meta["body"] = parts[2].strip()
                    for line in header_lines:
                        if ":" not in line:
                            continue
                        k, v = line.split(":", 1)
                        k = k.strip().lower()
                        v = v.strip()
                        if k == "name":
                            meta["name"] = v
                        elif k == "target_model":
                            meta["target_model"] = v
                        elif k == "triggers":
                            meta["triggers"] = [t.strip().lower() for t in v.split(",") if t.strip()]
                        elif k == "dsp_preset" and v.upper() != "NONE":
                            p_parts = [p.strip() for p in v.split("|")]
                            if len(p_parts) == 7:
                                meta["dsp_preset"] = [
                                    p_parts[0],
                                    float(p_parts[1]),
                                    float(p_parts[2]),
                                    float(p_parts[3]),
                                    float(p_parts[4]),
                                    float(p_parts[5]),
                                    float(p_parts[6]),
                                ]
            return meta
        except Exception:
            return None

    def route(self, user_prompt: str, prefer_mcp: bool = False):
        # Ricarica a caldo se l'utente aggiunge un nuovo .md durante la sessione
        self.reload_skills()
        q = (user_prompt or "").lower()
        best_skill = None
        best_score = 0

        for sk in self.skills:
            if prefer_mcp and sk["target_model"] != "qwen2.5-coder:3b":
                continue
            if not prefer_mcp and sk["target_model"] == "qwen2.5-coder:3b":
                continue
            score = sum(1 for trig in sk["triggers"] if trig in q)
            if score > best_score:
                best_score = score
                best_skill = sk

        if best_skill is None and self.skills:
            candidates = [s for s in self.skills if (s["target_model"] == "qwen2.5-coder:3b") == prefer_mcp]
            best_skill = candidates[0] if candidates else self.skills[0]

        return best_skill
