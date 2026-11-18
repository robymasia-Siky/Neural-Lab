import os
import json

MEMORY_FILE = os.path.join("ai_engine", "memory", "engineer_log.jsonl")
DATASET_FILE = os.path.join("training", "neural_lab_dataset.jsonl")

ENGINEER_SYS = (
    "Sei NEURAL LAB Copilot, un ingegnere del suono senior. Analizza la telemetria audio reale "
    "(LUFS, GR, bande di frequenza vs Reference e parametri DSP) e rispondi in italiano in massimo 2 frasi "
    "tecniche, concrete e misurate (Hz, dB, LUFS), senza JSON."
)

def distill_taste_memory_to_dataset():
    if not os.path.isfile(MEMORY_FILE):
        print("[DISTILL] Nessuna decisione salvata in ai_engine/memory/engineer_log.jsonl ancora.")
        return 0

    added = 0
    new_records = []
    with open(MEMORY_FILE, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            entry = json.loads(line)
            genre = entry.get("genre", "electronic")
            ref = entry.get("reference", "None")
            d = entry.get("dsp", {})
            note = entry.get("note", "custom")

            user_q = (
                f"Che impostazioni DSP consigli per questo brano {genre} (Reference: {ref})?\n"
                f"[LIVE_AUDIO: -8.5 LUFS | Limiter_GR=2.0dB | StudioPreference={note}]"
            )
            assistant_a = (
                f"In base al tuo storico su {genre}, imposta Sub Cut a {int(d.get('lowcut', 30))} Hz, "
                f"Input Drive a +{d.get('drive', 1.2)} dB, Neural Punch al {int(float(d.get('punch', 0.45)) * 100)}%, "
                f"Stereo Width al {int(float(d.get('width', 1.1)) * 100)}% e Ceiling a {d.get('ceiling', -0.3)} dB."
            )
            new_records.append({
                "messages": [
                    {"role": "system", "content": ENGINEER_SYS},
                    {"role": "user", "content": user_q},
                    {"role": "assistant", "content": assistant_a}
                ]
            })
            added += 1

    if new_records:
        with open(DATASET_FILE, "a", encoding="utf-8") as out:
            for r in new_records:
                out.write(json.dumps(r, ensure_ascii=False) + "\n")
        print(f"[DISTILL] Aggiunti {added} esempi personalizzati dalle tue scelte reali in {DATASET_FILE}!")
    return added

if __name__ == "__main__":
    distill_taste_memory_to_dataset()
