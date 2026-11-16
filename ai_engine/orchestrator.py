import json
import re
import urllib.request

class NeuralOrchestrator:
    def __init__(self, model_name: str = 'neural-lab-0.5b', fallback_model: str = 'qwen2.5:0.5b'):
        self.model_name = model_name
        self.fallback_model = fallback_model
        self.ollama_url = 'http://127.0.0.1:11434/api/generate'

    def _query_ollama(self, model: str, prompt: str) -> str:
        payload = json.dumps({
            'model': model,
            'prompt': prompt,
            'stream': False,
            'format': 'json',
            'options': {'temperature': 0.15, 'num_predict': 260}
        }).encode('utf-8')
        req = urllib.request.Request(self.ollama_url, data=payload, headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req, timeout=8) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            return data.get('response', '')

    def parse_prompt(self, user_prompt: str) -> dict:
        prompt_text = user_prompt.strip() or 'Dark Melodic Techno 126 BPM, Fm, punchy kick, -8.5 LUFS'
        raw = ''
        for m in [self.model_name, self.fallback_model]:
            try:
                raw = self._query_ollama(m, prompt_text)
                if raw:
                    break
            except Exception:
                continue

        try:
            match = re.search(r'\{[\s\S]*\}', raw)
            if match:
                parsed = json.loads(match.group(0))
                if 'sample_queries' in parsed and 'mix_target' in parsed:
                    return parsed
        except Exception:
            pass

        bpm_match = re.search(r'(\d{2,3})\s*bpm', prompt_text, re.IGNORECASE)
        bpm = int(bpm_match.group(1)) if bpm_match else 126
        key_match = re.search(r'\b([A-G][#b]?m)\b', prompt_text)
        key = key_match.group(1) if key_match else 'Fm'
        return {
            'genre': 'melodic_techno',
            'bpm': bpm,
            'key': key,
            'sample_queries': [
                {'role': 'KICK', 'type': 'kick', 'query': 'tight punchy melodic techno kick'},
                {'role': 'PERC', 'type': 'perc', 'query': 'crisp rolling 16th hi-hats and percussion'}
            ],
            'mix_target': {'target_lufs': -8.5, 'punch': 0.72, 'width': 1.18, 'drive': 1.5}
        }