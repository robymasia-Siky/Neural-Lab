import os
import threading
import numpy as np
import librosa
from pythonosc import dispatcher, osc_server, udp_client
from orchestrator import NeuralOrchestrator
from audio_indexer import SampleLibraryIndexer
from auto_master import NeuralMixMasterEngine

vst_client = udp_client.SimpleUDPClient("127.0.0.1", 9002)
ableton_client = udp_client.SimpleUDPClient("127.0.0.1", 11000)

orchestrator = NeuralOrchestrator(model_name="qwen2.5:0.5b")
indexer = SampleLibraryIndexer()
mix_engine = NeuralMixMasterEngine()

current_reference_profile = None

def send_status(text: str):
    print(f"[STATUS] {text}")
    vst_client.send_message("/neural_lab/status", [text])

def handle_add_folder(address, folder_path):
    def worker():
        send_status(f"CLAP: INDEXING {os.path.basename(folder_path)}...")
        total = indexer.add_folder_incremental(folder_path)
        send_status(f"LIBRARY READY: {total} SAMPLES INDEXED")
    threading.Thread(target=worker, daemon=True).start()

def handle_set_reference(address, ref_path):
    def worker():
        global current_reference_profile
        if not os.path.exists(ref_path):
            return
        send_status("MATCHERING: ANALYZING REFERENCE...")
        y, sr = librosa.load(ref_path, sr=22050, duration=30.0)
        rms = float(np.sqrt(np.mean(y**2))) + 1e-8
        ref_lufs = float(20.0 * np.log10(rms) - 0.691)
        S = np.abs(librosa.stft(y))
        centroid = float(np.mean(librosa.feature.spectral_centroid(S=S, sr=sr)))
        current_reference_profile = {"target_lufs": ref_lufs, "brightness": centroid, "path": ref_path}
        send_status(f"REF LOCKED: {round(ref_lufs, 1)} LUFS")
    threading.Thread(target=worker, daemon=True).start()

def handle_plugin_command(address, mode, prompt_text, ref_path=""):
    def worker():
        send_status("0.5B ORCHESTRATOR: ROUTING...")
        plan = orchestrator.parse_prompt(prompt_text)
        genre = plan.get("genre", "melodic_techno")
        bpm = float(plan.get("bpm", 126))
        mix_target = plan.get("mix_target", {})

        target_lufs = float(mix_target.get("target_lufs", -8.5))
        if current_reference_profile:
            target_lufs = current_reference_profile["target_lufs"]

        selected_paths = []

        if mode == "generate_track":
            send_status("CLAP AUDIO: MATCHING STEMS...")
            ableton_client.send_message("/live/song/set/tempo", [bpm])

            for idx, q in enumerate(plan.get("sample_queries", [])):
                matches = indexer.search_samples(q["query"], top_k=1, sample_type=q.get("type"))
                if matches:
                    sample = matches[0]
                    selected_paths.append(sample["path"])
                    # Invia lo stem scelto alla lista visiva dentro il Plugin VST3!
                    vst_client.send_message("/neural_lab/stem_selected", [
                        q["role"],
                        sample["filename"],
                        float(sample["similarity"])
                    ])
                    # Carica lo stem su Ableton Live
                    ableton_client.send_message("/live/song/create_audio_track", [-1])
                    ableton_client.send_message("/live/clip_slot/create_clip", [idx, 0, 16.0])
                    ableton_client.send_message("/live/clip/set/file_path", [idx, 0, sample["path"]])
                    ableton_client.send_message("/live/clip/fire", [idx, 0])

        dsp_params = mix_engine.compute_dsp_profile(
            genre=genre,
            target_lufs=target_lufs,
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

        if selected_paths:
            send_status(f"READY: {len(selected_paths)} STEMS @ {int(bpm)} BPM")
        else:
            send_status(f"MASTERED: {round(target_lufs, 1)} LUFS TARGET")

    threading.Thread(target=worker, daemon=True).start()

if __name__ == "__main__":
    disp = dispatcher.Dispatcher()
    disp.map("/neural_lab/command", handle_plugin_command)
    disp.map("/neural_lab/add_folder", handle_add_folder)
    disp.map("/neural_lab/set_reference", handle_set_reference)

    server = osc_server.ThreadingOSCUDPServer(("127.0.0.1", 9001), disp)
    print("NEURAL LAB AI ENGINE ATTIVO SU 127.0.0.1:9001")
    server.serve_forever()
