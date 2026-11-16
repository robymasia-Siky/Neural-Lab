import json
import random
import os

random.seed(42)

GENRES = {
    "melodic_techno": {
        "bpms": [123, 124, 125, 126, 127, 128],
        "keys": ["Fm", "Gm", "Cm", "Dm", "Am"],
        "lufs": [-8.8, -8.5, -8.2, -8.0],
        "punch": [0.65, 0.72, 0.78],
        "kicks": [
            "tight punchy melodic techno kick with clean low end",
            "deep analog sub kick 126 bpm club ready",
            "warm round techno kick drum minimal click"
        ],
        "percs": [
            "crisp rolling 16th hi-hats and metallic percussion loop",
            "hypnotic shaker groove and industrial rimshot percussion",
            "atmospheric dark ride and syncopated top loop"
        ],
        "synths": [
            "emotional analog saw arpeggio lead dark minor",
            "ethereal evolving pad and melancholic pluck synth",
            "gritty modular lead sequence afterhours style"
        ],
        "prompts_en": [
            "Dark melodic techno track at {bpm} BPM in {key}, punchy kick and rolling bass, master to {lufs} LUFS",
            "Afterlife style melodic techno {key} {bpm}bpm, emotional synth arp, tight club low end",
            "Hypnotic melodic techno groove in {key}, {bpm} BPM, wide stereo synths and clean sub",
            "Create a driving melodic techno beat {bpm} bpm {key} with crisp hats and deep punch"
        ],
        "prompts_it": [
            "Traccia melodic techno scura a {bpm} BPM in {key}, cassa potente e basso rolling, master a {lufs} LUFS",
            "Melodic techno stile club {key} {bpm} bpm, arpeggio emotivo e sub pulito",
            "Produci una base melodic techno ipnotica in {key} a {bpm} BPM con synth larghi"
        ]
    },
    "hypnotic_rave_techno": {
        "bpms": [130, 132, 134, 135, 138],
        "keys": ["Fm", "Gm", "Cm", "Dm"],
        "lufs": [-7.8, -7.5, -7.2],
        "punch": [0.80, 0.85, 0.90],
        "kicks": [
            "massive warehouse rumble techno kick 909 distortion",
            "heavy pounding rave kick drum high impact",
            "raw analog 909 techno kick with sub rumble"
        ],
        "percs": [
            "relentless 909 open hi-hat and industrial metallic loop",
            "driving rave ride cymbal and tribal techno percussion",
            "fast raw techno drum loop crunchy saturation"
        ],
        "synths": [
            "hypnotic acid 303 resonant sequence dark rave",
            "detuned warehouse rave stab chord minor",
            "relentless modular FM drone and dark synth pulse"
        ],
        "prompts_en": [
            "Carl Cox style hypnotic rave techno {bpm} BPM in {key}, heavy 909 rumble kick, {lufs} LUFS",
            "Raw warehouse rave techno {key} {bpm}bpm, relentless hats and acid synth",
            "High energy hypnotic techno track at {bpm} BPM, maximum punch and club loudness"
        ],
        "prompts_it": [
            "Techno rave ipnotica stile Carl Cox a {bpm} BPM in {key}, cassa 909 pesante, {lufs} LUFS",
            "Traccia rave warehouse {key} {bpm} bpm con hi-hat serrati e synth acido"
        ]
    },
    "deep_house": {
        "bpms": [120, 121, 122, 123, 124],
        "keys": ["Am", "Dm", "Gm", "Cm", "Fm"],
        "lufs": [-9.5, -9.2, -9.0],
        "punch": [0.48, 0.55, 0.60],
        "kicks": [
            "warm deep house kick soft transient round sub",
            "velvety analog 808 909 hybrid deep house kick",
            "smooth organic house kick drum warm low end"
        ],
        "percs": [
            "shuffled deep house hi-hat groove and organic shakers",
            "warm rimshot clap and dusty vinyl percussion loop",
            "smooth afro-deep percussion groove with crisp tops"
        ],
        "synths": [
            "warm Rhodes minor 7th dub chord stab with delay",
            "lush analog Juno pad and soft plucked bassline",
            "late night deep house organ chord progression"
        ],
        "prompts_en": [
            "Warm late night deep house in {key} at {bpm} BPM, dusty chords and round sub, {lufs} LUFS",
            "Smooth minimal deep house groove {bpm}bpm {key}, organic percussion and warm analog feel",
            "Deep house club track {key} {bpm} BPM with lush chords and gentle compression"
        ],
        "prompts_it": [
            "Deep house calda notturna in {key} a {bpm} BPM, accordi dub e sub rotondo, {lufs} LUFS",
            "Groove deep house minimal {bpm} bpm {key} con percussioni organiche e calore analogico"
        ]
    }
}

SYSTEM_PROMPT = (
    "You are NEURAL LAB's 0.5B AI Studio Orchestrator. Convert the producer's natural language request "
    "into a strict, valid JSON production plan with keys: genre, bpm, key, sample_queries (list of role/type/query), "
    "midi_style, and mix_target (target_lufs, punch, width, drive). Output ONLY raw JSON."
)

def generate_dataset(output_path: str, count: int = 2000):
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    records = []
    genre_names = list(GENRES.keys())

    for i in range(count):
        g_name = genre_names[i % len(genre_names)]
        cfg = GENRES[g_name]
        bpm = random.choice(cfg["bpms"])
        key = random.choice(cfg["keys"])
        lufs = random.choice(cfg["lufs"])
        punch = random.choice(cfg["punch"])
        width = round(random.uniform(1.08, 1.32), 2)
        drive = round(random.uniform(0.8, 2.8), 1)

        pool = cfg["prompts_en"] * 3 + cfg["prompts_it"]
        user_prompt = random.choice(pool).format(bpm=bpm, key=key, lufs=lufs)

        target_json = {
            "genre": g_name,
            "bpm": bpm,
            "key": key,
            "sample_queries": [
                {"role": "KICK", "type": "kick", "query": random.choice(cfg["kicks"])},
                {"role": "PERC", "type": "perc", "query": random.choice(cfg["percs"])},
                {"role": "SYNTH", "type": "synth", "query": random.choice(cfg["synths"])}
            ],
            "midi_style": "rolling_bass_and_arp" if g_name != "deep_house" else "dub_chords_and_sub",
            "mix_target": {
                "target_lufs": lufs,
                "punch": punch,
                "width": width,
                "drive": drive
            }
        }

        records.append({
            "messages": [
                {"role": "system", "content": SYSTEM_PROMPT},
                {"role": "user", "content": user_prompt},
                {"role": "assistant", "content": json.dumps(target_json, separators=(",", ":"))}
            ]
        })

    with open(output_path, "w", encoding="utf-8") as f:
        for r in records:
            f.write(json.dumps(r, ensure_ascii=False) + "\n")
    print(f"[DATASET] Generated {len(records)} studio prompt->JSON pairs at {output_path}")

if __name__ == "__main__":
    generate_dataset("training/neural_lab_dataset.jsonl", count=2000)