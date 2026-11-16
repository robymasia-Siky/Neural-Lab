import os
from pythonosc import dispatcher, osc_server, udp_client
from orchestrator import NeuralOrchestrator
from audio_indexer import SampleLibraryIndexer
from auto_master import NeuralMixMasterEngine

vst_client = udp_client.SimpleUDPClient("127.0.0.1", 9002)
ableton_client = udp_client.SimpleUDPClient("127.0.0.1", 11000)

# Orchestratore ultra-leggero da 0.5B (~400MB VRAM, latenza < 50ms)
orchestrator = NeuralOrchestrator(model_name="qwen2.5:0.5b")
indexer = SampleLibraryIndexer()
mix_engine = NeuralMixMasterEngine()

def send_status(text: str):
    print(f"[STATUS] {text}")
    vst_client.send_message("/neural_lab/status", [text])

def handle_plugin_command(address, mode, prompt_text):
    print(f"\n>>> Comando VST3 | Mode: {mode} | Prompt: '{prompt_text}'")
    send_status("0.5B ORCHESTRATOR: ROUTING...")

    plan = orchestrator.parse_prompt(prompt_text)
    genre = plan.get("genre", "melodic_techno")
    bpm = float(plan.get("bpm", 126))
    mix_target = plan.get("mix_target", {})

    selected_paths = []
    selected_info = []

    if mode == "generate_track":
        send_status("CLAP AUDIO: SELECTING STEMS...")
        ableton_client.send_message("/live/song/set/tempo", [bpm])

        for idx, q in enumerate(plan.get("sample_queries", [])):
            matches = indexer.search_samples(q["query"], top_k=1, sample_type=q.get("type"))
            if matches:
                sample = matches[0]
                selected_paths.append(sample["path"])
                selected_info.append((q["role"], sample["filename"], sample["path"]))
                print(f"  [Track {idx+1} - {q['role'].upper()}] -> {sample['filename']} (sim: {sample['similarity']})")

                # Crea traccia audio su Ableton Live e carica il file .wav nello slot 0
                ableton_client.send_message("/live/song/create_audio_track", [-1])
                ableton_client.send_message("/live/clip_slot/create_clip", [idx, 0, 16.0])
                ableton_client.send_message("/live/clip/set/file_path", [idx, 0, sample["path"]])
                ableton_client.send_message("/live/clip/fire", [idx, 0])

    # Analisi acustica reale sugli stems scelti + aggiornamento catena DSP del plugin VST3
    send_status("NEURAL DSP: COMPUTING MIX & MASTER...")
    dsp_params = mix_engine.compute_dsp_profile(
        genre=genre,
        target_lufs=float(mix_target.get("target_lufs", -8.5)),
        punch_intent=float(mix_target.get("punch", 0.5)),
        stem_paths=selected_paths
    )

    vst_client.send_message("/neural_lab/set_dsp", [
        dsp_params["lowcut"],
        dsp_params["punch"],
        dsp_params["width"],
        dsp_params["drive"],
        dsp_params["ceiling"]
    ])

    if selected_info:
        send_status(f"ACTIVE: {len(selected_info)} STEMS @ {int(bpm)} BPM | MASTERED")
    else:
        send_status(f"DSP CALIBRATED: {genre.upper()} ({mix_target.get('target_lufs', -8.5)} LUFS)")

if __name__ == "__main__":
    disp = dispatcher.Dispatcher()
    disp.map("/neural_lab/command", handle_plugin_command)

    server = osc_server.ThreadingOSCUDPServer(("127.0.0.1", 9001), disp)
    print("=================================================================")
    print(" NEURAL LAB AI ENGINE (0.5B Orchestrator + CLAP + Spectral DSP)")
    print(" In ascolto su 127.0.0.1:9001")
    print("=================================================================")
    server.serve_forever()
