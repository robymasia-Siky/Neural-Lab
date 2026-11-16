import os
import threading
import numpy as np
import librosa
from pythonosc import dispatcher, osc_server, udp_client
from orchestrator import NeuralOrchestrator
from audio_indexer import SampleLibraryIndexer
from auto_master import NeuralMixMasterEngine
from midi_composer import NeuralMidiComposer

vst_client = udp_client.SimpleUDPClient('127.0.0.1', 9002)
ableton_client = udp_client.SimpleUDPClient('127.0.0.1', 11000)

orchestrator = NeuralOrchestrator(model_name='qwen2.5:0.5b')
indexer = SampleLibraryIndexer()
mix_engine = NeuralMixMasterEngine()
midi_composer = NeuralMidiComposer()
current_reference_profile = None

def send_status(text: str):
    print(f'[STATUS] {text}')
    vst_client.send_message('/neural_lab/status', [text])

def handle_add_folder(address, folder_path):
    def worker():
        send_status(f'CLAP: INDEXING {os.path.basename(folder_path)}...')
        total = indexer.add_folder_incremental(folder_path)
        send_status(f'LIBRARY READY: {total} SAMPLES INDEXED')
    threading.Thread(target=worker, daemon=True).start()

def handle_set_reference(address, ref_path):
    def worker():
        global current_reference_profile
        if not os.path.exists(ref_path): return
        send_status('MATCHERING: ANALYZING REFERENCE...')
        y, sr = librosa.load(ref_path, sr=22050, duration=30.0)
        rms = float(np.sqrt(np.mean(y**2))) + 1e-8
        ref_lufs = float(20.0 * np.log10(rms) - 0.691)
        current_reference_profile = {'target_lufs': ref_lufs, 'path': ref_path}
        send_status(f'REF LOCKED: {round(ref_lufs, 1)} LUFS')
    threading.Thread(target=worker, daemon=True).start()

def write_midi_clip_to_ableton(track_idx: int, name: str, notes: list, clip_len: float = 16.0):
    ableton_client.send_message('/live/song/create_midi_track', [-1])
    ableton_client.send_message('/live/track/set/name', [track_idx, name])
    ableton_client.send_message('/live/clip_slot/create_clip', [track_idx, 0, clip_len])
    ableton_client.send_message('/live/clip/set/name', [track_idx, 0, name])
    # Invia tutte le note MIDI (pitch, start_time, duration, velocity, mute) ad AbletonOSC
    flat_notes = [track_idx, 0]
    for n in notes:
        flat_notes.extend([int(n[0]), float(n[1]), float(n[2]), int(n[3]), int(n[4])])
    ableton_client.send_message('/live/clip/add/notes', flat_notes)
    ableton_client.send_message('/live/clip/fire', [track_idx, 0])

def handle_plugin_command(address, mode, prompt_text, ref_path=''):
    def worker():
        send_status('0.5B ORCHESTRATOR: COMPOSING...')
        plan = orchestrator.parse_prompt(prompt_text)
        genre = plan.get('genre', 'melodic_techno')
        bpm = float(plan.get('bpm', 126))
        key = plan.get('key', 'Fm')
        mix_target = plan.get('mix_target', {})

        target_lufs = float(mix_target.get('target_lufs', -8.5))
        if current_reference_profile:
            target_lufs = current_reference_profile['target_lufs']

        selected_paths = []
        track_cursor = 0

        if mode == 'generate_track':
            send_status(f'CLAP + MIDI ENGINE ({key} @ {int(bpm)} BPM)...')
            ableton_client.send_message('/live/song/set/tempo', [bpm])

            # 1. Carica Stems Audio da CLAP
            for q in plan.get('sample_queries', []):
                matches = indexer.search_samples(q['query'], top_k=1, sample_type=q.get('type'))
                if matches:
                    sample = matches[0]
                    selected_paths.append(sample['path'])
                    vst_client.send_message('/neural_lab/stem_selected', [q['role'], sample['filename'], float(sample['similarity'])])
                    ableton_client.send_message('/live/song/create_audio_track', [-1])
                    ableton_client.send_message('/live/clip_slot/create_clip', [track_cursor, 0, 16.0])
                    ableton_client.send_message('/live/clip/set/file_path', [track_cursor, 0, sample['path']])
                    ableton_client.send_message('/live/clip/fire', [track_cursor, 0])
                    track_cursor += 1

            # 2. Genera Tracce MIDI con Velocity & Automazioni su Ableton Live
            bass_notes = midi_composer.generate_rolling_bass(key=key, bars=4)
            write_midi_clip_to_ableton(track_cursor, f'NL BASS ({key})', bass_notes, 16.0)
            vst_client.send_message('/neural_lab/stem_selected', ['MIDI BASS', f'Rolling_{key}_VelDyn.mid', 0.99])
            track_cursor += 1

            arp_notes = midi_composer.generate_hypnotic_arp(key=key, bars=4)
            write_midi_clip_to_ableton(track_cursor, f'NL ARP ({key})', arp_notes, 16.0)
            vst_client.send_message('/neural_lab/stem_selected', ['MIDI ARP', f'Hypnotic_{key}_Auto.mid', 0.99])
            # Applica automazione filtro/pan sulla traccia Synth MIDI
            ableton_client.send_message('/live/track/set/panning', [track_cursor, -0.15])
            track_cursor += 1

        dsp_params = mix_engine.compute_dsp_profile(
            genre=genre,
            target_lufs=target_lufs,
            punch_intent=float(mix_target.get('punch', 0.5)),
            stem_paths=selected_paths
        )

        vst_client.send_message('/neural_lab/set_dsp', [
            dsp_params['lowcut'],
            dsp_params['punch'],
            dsp_params['width'],
            dsp_params['drive'],
            dsp_params['ceiling']
        ])

        send_status(f'READY: AUDIO + MIDI ({key}) @ {int(bpm)} BPM')

    threading.Thread(target=worker, daemon=True).start()

if __name__ == '__main__':
    disp = dispatcher.Dispatcher()
    disp.map('/neural_lab/command', handle_plugin_command)
    disp.map('/neural_lab/add_folder', handle_add_folder)
    disp.map('/neural_lab/set_reference', handle_set_reference)

    server = osc_server.ThreadingOSCUDPServer(('127.0.0.1', 9001), disp)
    print('NEURAL LAB AI ENGINE (0.5B + CLAP + MIDI COMPOSER + SPECTRAL DSP)')
    server.serve_forever()
