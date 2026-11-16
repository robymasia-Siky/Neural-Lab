import threading
from pythonosc import dispatcher, osc_server, udp_client
from orchestrator import NeuralOrchestrator
from audio_indexer import SampleLibraryIndexer
from auto_master import NeuralMixMasterEngine

# Porte di comunicazione locale:
# 9001: Riceve comandi dal Plugin VST3 NEURAL LAB
# 9002: Invia parametri DSP e status al Plugin VST3 NEURAL LAB
# 11000: Invia comandi ad Ableton Live (via AbletonOSC)

vst_client = udp_client.SimpleUDPClient("127.0.0.1", 9002)
ableton_client = udp_client.SimpleUDPClient("127.0.0.1", 11000)

orchestrator = NeuralOrchestrator(model_name="qwen2.5:0.5b") # Sostituibile con lfm-1.7b custom
indexer = SampleLibraryIndexer()
mix_engine = NeuralMixMasterEngine()

def send_status(text: str):
    print(f"[STATUS] {text}")
    vst_client.send_message("/neural_lab/status", [text])

def handle_plugin_command(address, mode, prompt_text):
    print(f"\n>>> Comando ricevuto dal VST3 | Mode: {mode} | Prompt: '{prompt_text}'")
    send_status("ORCHESTRATOR: ANALYZING INTENT...")

    plan = orchestrator.parse_prompt(prompt_text)
    genre = plan.get("genre", "melodic_techno")
    bpm = float(plan.get("bpm", 126))
    mix_target = plan.get("mix_target", {})

    # 1. Calcolo e invio parametri Auto-Mix & Master al Plugin VST3
    dsp_params = mix_engine.compute_dsp_profile(
        genre=genre,
        target_lufs=float(mix_target.get("target_lufs", -8.5)),
        punch_intent=float(mix_target.get("punch", 0.5))
    )
    vst_client.send_message("/neural_lab/set_dsp", [
        dsp_params["lowcut"],
        dsp_params["punch"],
        dsp_params["width"],
        dsp_params["drive"],
        dsp_params["ceiling"]
    ])

    # 2. Se richiesto 'generate_track', cerca i sample con CLAP e pilota Ableton Live
    if mode == "generate_track":
        send_status("CLAP: SEARCHING MATCHING SAMPLES...")
        ableton_client.send_message("/live/song/set/tempo", [bpm])

        selected_tracks = []
        for idx, q in enumerate(plan.get("sample_queries", [])):
            matches = indexer.search_samples(q["query"], top_k=1, sample_type=q.get("type"))
            if matches:
                sample = matches[0]
                selected_tracks.append((q["role"], sample["filename"], sample["path"]))
                print(f"  [Track {idx+1} - {q['role'].upper()}] -> {sample['filename']} (sim: {sample['similarity']})")
                # Crea traccia audio su Ableton Live via AbletonOSC
                ableton_client.send_message("/live/song/create_audio_track", [-1])

        if selected_tracks:
            send_status(f"LOADED {len(selected_tracks)} STEMS @ {int(bpm)} BPM")
        else:
            send_status("DSP MASTER APPLIED (INDEX SAMPLES FIRST)")
    else:
        send_status(f"MASTER PROFILE: {genre.upper()} ({mix_target.get('target_lufs', -8.5)} LUFS)")

if __name__ == "__main__":
    disp = dispatcher.Dispatcher()
    disp.map("/neural_lab/command", handle_plugin_command)

    server = osc_server.ThreadingOSCUDPServer(("127.0.0.1", 9001), disp)
    print("=================================================================")
    print(" NEURAL LAB AI ENGINE ATTIVO (GPU + Ollama + OSC Bridge)")
    print(" In ascolto dal Plugin VST3 su 127.0.0.1:9001")
    print("=================================================================")
    server.serve_forever()
