import json
import requests

SYSTEM_PROMPT = """Sei il cervello orchestratore di NEURAL LAB, un sistema di produzione musicale e auto-mastering.
Il tuo UNICO compito e' tradurre la richiesta dell'utente in un oggetto JSON valido e rigoroso.
Non scrivere MAI spiegazioni o testo fuori dal JSON.

Schema JSON obbligatorio:
{
  "genre": "stringa (es. melodic_techno, deep_house, rave_techno)",
  "bpm": numero (es. 126),
  "key": "stringa (es. Fm, Am, Cm)",
  "sample_queries": [
    {"role": "kick", "query": "descrizione timbrica per CLAP", "type": "one_shot"},
    {"role": "bass", "query": "descrizione basso per CLAP", "type": "loop"},
    {"role": "synth", "query": "descrizione synth/arp per CLAP", "type": "loop"},
    {"role": "perc", "query": "descrizione hihat/perc per CLAP", "type": "loop"}
  ],
  "mix_target": {
    "target_lufs": numero (es. -8.5),
    "punch": numero da 0.0 a 1.0,
    "atmosphere": "dark | bright | warm"
  }
}"""

class NeuralOrchestrator:
    def __init__(self, model_name="qwen2.5:0.5b", ollama_url="http://127.0.0.1:11434/api/generate"):
        self.model_name = model_name
        self.ollama_url = ollama_url

    def parse_prompt(self, user_prompt: str) -> dict:
        prompt_text = user_prompt.strip() or "Dark Melodic Techno 126 BPM, punchy low-end, -8.5 LUFS"
        payload = {
            "model": self.model_name,
            "prompt": f"{SYSTEM_PROMPT}\n\nRichiesta utente: {prompt_text}\nJSON:",
            "format": "json",
            "stream": False,
            "options": {"temperature": 0.2, "num_predict": 350}
        }
        try:
            resp = requests.post(self.ollama_url, json=payload, timeout=15)
            resp.raise_for_status()
            raw_json = resp.json().get("response", "{}")
            return json.loads(raw_json)
        except Exception as e:
            print(f"[Orchestrator Fallback] Uso profilo default ({e})")
            return {
                "genre": "melodic_techno",
                "bpm": 126,
                "key": "Fm",
                "sample_queries": [
                    {"role": "kick", "query": "punchy dark analog techno kick", "type": "one_shot"},
                    {"role": "bass", "query": "rolling dark sub bassline", "type": "loop"},
                    {"role": "synth", "query": "hypnotic minor arpeggio synth", "type": "loop"},
                    {"role": "perc", "query": "crisp 909 closed hihat groove", "type": "loop"}
                ],
                "mix_target": {"target_lufs": -8.5, "punch": 0.65, "atmosphere": "dark"}
            }
