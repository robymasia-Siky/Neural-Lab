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

ORCHESTRATOR_SYS = (
    "You are NEURAL LAB's 0.5B AI Studio Orchestrator. Convert the producer's natural language request "
    "into a strict, valid JSON production plan with keys: genre, bpm, key, sample_queries (list of role/type/query), "
    "midi_style, and mix_target (target_lufs, punch, width, drive). Output ONLY raw JSON."
)

ENGINEER_SYS = (
    "Sei NEURAL LAB Copilot, un ingegnere del suono senior. Analizza la telemetria audio reale "
    "(LUFS, GR, bande di frequenza vs Reference e parametri DSP) e rispondi in italiano in massimo 2 frasi "
    "tecniche, concrete e misurate (Hz, dB, LUFS), senza JSON."
)

ENGINEER_SCENARIOS = [
    {
        "q": ["Come suona il master? Il kick perde definizione sul drop.", "Il basso copre la cassa, come pulisco il low end?"],
        "tele": "LIVE_AUDIO: -7.1 LUFS | Limiter_GR=5.4dB | DSP[Drive=+3.2dB, SubCut=24Hz, Punch=72%, Width=110%, EQMatch=40%, Ceiling=-0.1dB] | BandLevels[Sub45Hz=-6.2dB(vsRef:+3.1dB), LowMid260Hz=-11.4dB(vsRef:+2.8dB), Air8k=-21.0dB(vsRef:-1.4dB)]",
        "ans": "Stai spingendo troppo il limiter (5.4 dB di GR) con +3.1 dB di sub in eccesso e accumulo a 260 Hz: abbassa Drive a +1.2 dB, porta Sub Cut a 34 Hz e scava i medio-bassi con EQ Match al 70%."
    },
    {
        "q": ["Il mix suona inscatolato e impastato sulle medie.", "C'è troppo fango tra basso e synth, cosa faccio?"],
        "tele": "LIVE_AUDIO: -9.4 LUFS | Limiter_GR=1.8dB | DSP[Drive=+1.0dB, SubCut=30Hz, Punch=45%, Width=105%, EQMatch=35%, Ceiling=-0.3dB] | BandLevels[Sub45Hz=-9.5dB(vsRef:+0.3dB), LowMid260Hz=-9.8dB(vsRef:+3.6dB), Pres3.6k=-18.5dB(vsRef:-2.1dB)]",
        "ans": "Hai un accumulo netto di +3.6 dB a 260 Hz e carenza di presenza a 3.6 kHz rispetto alla reference: attenua di 3 dB la zona 240-300 Hz e alza EQ Match al 75% per aprire i lead."
    },
    {
        "q": ["Voglio più aria e larghezza stereo senza perdere impatto in mono.", "Come rendo i pad e gli arp più larghi stile Melodic Techno?"],
        "tele": "LIVE_AUDIO: -8.6 LUFS | Limiter_GR=2.2dB | DSP[Drive=+1.4dB, SubCut=30Hz, Punch=50%, Width=100%, EQMatch=50%, Ceiling=-0.3dB] | BandLevels[Sub45Hz=-8.8dB(vsRef:-0.2dB), Air8k=-23.4dB(vsRef:-2.9dB), Bril14k=-26.1dB(vsRef:-2.4dB)]",
        "ans": "Mancano circa 2.9 dB sull'aria sopra gli 8 kHz: porta Stereo Width al 125% (il Club Mono-Maker tiene il sub sotto 120 Hz perfettamente mono) e alza EQ Match al 70% per illuminare le alte."
    },
    {
        "q": ["Posso spingere ancora il volume per farlo suonare forte come una traccia club?", "A quanti LUFS sono e quanto margine ho ancora?"],
        "tele": "LIVE_AUDIO: -10.8 LUFS | Limiter_GR=0.9dB | DSP[Drive=+0.4dB, SubCut=28Hz, Punch=38%, Width=108%, EQMatch=55%, Ceiling=-0.3dB] | BandLevels[Sub45Hz=-10.2dB(vsRef:-0.5dB), LowMid260Hz=-14.0dB(vsRef:+0.2dB), Air8k=-20.1dB(vsRef:-0.3dB)]",
        "ans": "Sei a -10.8 LUFS con appena 0.9 dB di GR e spettro ben bilanciato: alza Input Drive a +2.4 dB e Neural Punch al 62% sfruttando il soft-clipper 4x oversampled per arrivare a -8 LUFS puliti."
    },
    {
        "q": ["Sento le alte frequenze troppo aspre e affaticanti sul drop.", "Come ammorbidisco i piatti e il lead senza spegnere il brano?"],
        "tele": "LIVE_AUDIO: -7.9 LUFS | Limiter_GR=3.6dB | DSP[Drive=+2.5dB, SubCut=32Hz, Punch=65%, Width=118%, EQMatch=45%, Ceiling=-0.2dB] | BandLevels[Pres3.6k=-12.1dB(vsRef:+3.4dB), Air8k=-15.8dB(vsRef:+2.2dB)]",
        "ans": "Hai un picco di +3.4 dB sulla presenza a 3.6 kHz che fa scattare il limiter in anticipo: riduci Drive a +1.2 dB, abbassa Neural Punch al 48% e attiva EQ Match all'80% per domare le risonanze."
    },
    {
        "q": ["Il sub-basso non spinge abbastanza sull'impianto.", "Come do più peso al basso e alla cassa?"],
        "tele": "LIVE_AUDIO: -9.6 LUFS | Limiter_GR=1.5dB | DSP[Drive=+0.8dB, SubCut=48Hz, Punch=40%, Width=112%, EQMatch=30%, Ceiling=-0.3dB] | BandLevels[Sub45Hz=-14.5dB(vsRef:-4.2dB), Bass110Hz=-11.8dB(vsRef:-1.9dB)]",
        "ans": "Il filtro Sub Cut a 48 Hz sta tagliando la fondamentale del sub (-4.2 dB rispetto alla reference): riporta Sub Cut a 28-30 Hz e porta Neural Punch al 58% per recuperare corpo e impatto."
    }
]

def generate_dataset(output_path: str, orchestrator_count: int = 2000, engineer_count: int = 1000):
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    records = []
    genre_names = list(GENRES.keys())

    # 1. Orchestrator JSON pairs
    for i in range(orchestrator_count):
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
                {"role": "system", "content": ORCHESTRATOR_SYS},
                {"role": "user", "content": user_prompt},
                {"role": "assistant", "content": json.dumps(target_json, separators=(",", ":"))}
            ]
        })

    # 2. Senior Sound & Mastering Engineer diagnostic pairs
    for j in range(engineer_count):
        sc = ENGINEER_SCENARIOS[j % len(ENGINEER_SCENARIOS)]
        question = random.choice(sc["q"])
        user_msg = f"{question}\n[{sc['tele']}]"
        records.append({
            "messages": [
                {"role": "system", "content": ENGINEER_SYS},
                {"role": "user", "content": user_msg},
                {"role": "assistant", "content": sc["ans"]}
            ]
        })

    random.shuffle(records)
    with open(output_path, "w", encoding="utf-8") as f:
        for r in records:
            f.write(json.dumps(r, ensure_ascii=False) + "\n")
    print(f"[DATASET] Generated {len(records)} multi-task records ({orchestrator_count} Orchestrator + {engineer_count} Sound Engineer) at {output_path}")

if __name__ == "__main__":
    generate_dataset("training/neural_lab_dataset.jsonl", orchestrator_count=2000, engineer_count=1000)
