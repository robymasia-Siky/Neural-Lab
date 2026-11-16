import os
import sys
import threading
import traceback
import numpy as np
from pythonosc import dispatcher, osc_server, udp_client

try:
    from install_ableton_osc import ensure_ableton_osc_installed
    ensure_ableton_osc_installed()
except Exception as e:
    print(f"[AbletonOSC init warning] {e}")

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
active_session_state = {'key': 'Fm', 'bpm': 126.0, 'stems': [], 'swap_offsets': {}}

def send_status(text: str):
    print(f'[STATUS] {text}', flush=True)
    vst_client.send_message('/neural_lab/status', [str(text)])

def load_audio_fast(filepath: str, max_seconds: int = 30):
    # Tentativo 1: soundfile (WAV, FLAC, OGG, AIFF - velocissimo)
    try:
        import soundfile as sf
        info = sf.info(filepath)
        sr = info.samplerate
        frames_to_read = int(sr * max_seconds)
        data, _ = sf.read(filepath, frames=frames_to_read, dtype='float32', always_2d=True)
        return np.mean(data, axis=1), sr
    except Exception as e1:
        print(f"[soundfile fallback] {e1}", flush=True)

    # Tentativo 2: librosa / audioread (MP3, M4A, ecc.)
    import librosa
    y, sr = librosa.load(filepath, sr=22050, duration=float(max_seconds), mono=True)
    return y, sr

def handle_add_folder(address, folder_path):
    def worker():
        try:
            send_status(f'CLAP: INDEXING {os.path.basename(folder_path)}...')
            total = indexer.add_folder_incremental(folder_path)
            send_status(f'LIBRARY READY: {total} SAMPLES INDEXED')
        except Exception as e:
            traceback.print_exc()
            send_status(f'INDEX ERR: {str(e)[:28]}')
    threading.Thread(target=worker, daemon=True).start()

def handle_set_reference(address, ref_path):
    def worker():
        global current_reference_profile
        try:
            clean_path = os.path.normpath(str(ref_path).strip().strip('"'))
            print(f"[REF] Loading: {clean_path}", flush=True)
            if not os.path.exists(clean_path):
                send_status('REF ERR: FILE NOT FOUND')
                return

            send_status('MATCHERING: ANALYZING AUDIO...')
            y, sr = load_audio_fast(clean_path, max_seconds=30)

            if y is None or len(y) < 2048:
                send_status('REF ERR: AUDIO TOO SHORT')
                return

            rms = float(np.sqrt(np.mean(y ** 2))) + 1e-8
            ref_lufs = float(20.0 * np.log10(rms) - 0.691)

            n_fft = 2048
            win = np.hanning(n_fft).astype(np.float32)
            hop = 2048
            num_frames = (len(y) - n_fft) // hop
            if num_frames <= 0:
                specs = [np.abs(np.fft.rfft(y[:n_fft] * win))]
            else:
                step_stride = max(1, num_frames // 120)
                specs = []
                for idx in range(0, num_frames, step_stride):
                    start = idx * hop
                    frame = y[start:start + n_fft]
                    specs.append(np.abs(np.fft.rfft(frame * win)))

            mean_spec = np.mean(specs, axis=0)
            max_val = float(np.max(mean_spec)) + 1e-8
            db_spec = 20.0 * np.log10(mean_spec / max_val + 1e-8)

            curve = []
            for i in range(64):
                b = min(int(1.09 ** i), len(db_spec) - 1)
                norm = float((db_spec[b] + 55.0) / 55.0)
                curve.append(float(np.clip(norm, 0.08, 0.92)))

            current_reference_profile = {
                'target_lufs': ref_lufs,
                'path': clean_path,
                'curve': curve
            }

            vst_client.send_message('/neural_lab/ref_curve', [float(v) for v in curve])
            send_status(f'REF LOCKED: {round(ref_lufs, 1)} LUFS + EQ CURVE')
            print(f"[REF] Done! LUFS={round(ref_lufs, 1)}", flush=True)

        except Exception as exc:
            traceback.print_exc()
            send_status(f'REF ERR: {str(exc)[:30]}')

    threading.Thread(target=worker, daemon=True).start()

def write_midi_clip_to_ableton(track_idx: int, scene_idx: int, name: str, notes: list, clip_len: float = 16.0, create_track: bool = True):
    if create_track:
        ableton_client.send_message('/live/song/create_midi_track', [-1])
        ableton_client.send_message('/live/track/set/name', [track_idx, name])
    ableton_client.send_message('/live/clip_slot/create_clip', [track_idx, scene_idx, clip_len])
    ableton_client.send_message('/live/clip/set/name', [track_idx, scene_idx, name])
    flat_notes = [track_idx, scene_idx]
    for n in notes:
        flat_notes.extend([int(n[0]), float(n[1]), float(n[2]), int(n[3]), int(n[4])])
    ableton_client.send_message('/live/clip/add/notes', flat_notes)

def build_full_club_arrangement(key: str, bpm: float):
    sections = [
        ('1. INTRO (16 BARS)',    [0, 3]),
        ('2. BUILD-UP (16 BARS)', [0, 1, 2, 3]),
        ('3. DROP 1 (32 BARS)',   [0, 1, 2, 3]),
        ('4. BREAKDOWN (16 BARS)',[2, 3]),
        ('5. DROP 2 (32 BARS)',   [0, 1, 2, 3]),
        ('6. OUTRO (16 BARS)',    [0, 3])
    ]
    for s_idx, (sec_name, active_tracks) in enumerate(sections):
        if s_idx > 0:
            ableton_client.send_message('/live/song/create_scene', [-1])
        ableton_client.send_message('/live/scene/set/name', [s_idx, sec_name])
        for stem in active_session_state['stems']:
            t_idx = stem['track_idx']
            if t_idx in active_tracks:
                if stem['is_midi']:
                    notes = midi_composer.generate_rolling_bass(key, 4) if 'BASS' in stem['role'] else midi_composer.generate_hypnotic_arp(key, 4)
                    clip_label = f"{stem['role']} ({sec_name[:7]})"
                    write_midi_clip_to_ableton(t_idx, s_idx, clip_label, notes, 16.0, create_track=False)
                elif stem.get('path'):
                    ableton_client.send_message('/live/clip_slot/create_clip', [t_idx, s_idx, 16.0])
                    ableton_client.send_message('/live/clip/set/file_path', [t_idx, s_idx, stem['path']])
    ableton_client.send_message('/live/scene/fire', [2])

def handle_swap_stem(address, stem_idx, role):
    def worker():
        try:
            offset = active_session_state['swap_offsets'].get(stem_idx, 0) + 1
            active_session_state['swap_offsets'][stem_idx] = offset
            key = active_session_state['key']
            if 'MIDI' in role.upper():
                notes = midi_composer.generate_rolling_bass(key, 4) if 'BASS' in role.upper() else midi_composer.generate_hypnotic_arp(key, 4)
                clean_role = role.replace('MIDI ', '')
                new_name = f"{clean_role}_{key}_Var{offset}.mid"
                write_midi_clip_to_ableton(stem_idx, 0, new_name, notes, 16.0, create_track=False)
                ableton_client.send_message('/live/clip/fire', [stem_idx, 0])
                vst_client.send_message('/neural_lab/stem_selected', [stem_idx, role, new_name, 0.99])
                send_status(f'SWAPPED {role}: NEW VARIATION #{offset}')
            else:
                query = f'dark melodic techno {role.lower()}'
                matches = indexer.search_samples(query, top_k=offset + 2)
                if len(matches) > offset:
                    sample = matches[offset]
                    ableton_client.send_message('/live/clip/set/file_path', [stem_idx, 0, sample['path']])
                    ableton_client.send_message('/live/clip/fire', [stem_idx, 0])
                    vst_client.send_message('/neural_lab/stem_selected', [stem_idx, role, sample['filename'], float(sample['similarity'])])
                    send_status(f"SWAPPED {role} -> {sample['filename']}")
        except Exception as e:
            traceback.print_exc()
            send_status(f'SWAP ERR: {str(e)[:28]}')
    threading.Thread(target=worker, daemon=True).start()

def handle_plugin_command(address, mode, prompt_text, ref_path=''):
    def worker():
        try:
            send_status('0.5B ORCHESTRATOR: PLANNING...')
            plan = orchestrator.parse_prompt(prompt_text)
            genre = plan.get('genre', 'melodic_techno')
            bpm = float(plan.get('bpm', 126))
            key = plan.get('key', 'Fm')
            mix_target = plan.get('mix_target', {})
            active_session_state['key'] = key
            active_session_state['bpm'] = bpm

            target_lufs = float(mix_target.get('target_lufs', -8.5))
            if current_reference_profile:
                target_lufs = current_reference_profile['target_lufs']

            selected_paths = []
            if mode in ['generate_track', 'arrange_full']:
                active_session_state['stems'] = []
                active_session_state['swap_offsets'] = {}
                ableton_client.send_message('/live/song/set/tempo', [bpm])
                track_cursor = 0
                for q in plan.get('sample_queries', [])[:2]:
                    matches = indexer.search_samples(q['query'], top_k=1, sample_type=q.get('type'))
                    if matches:
                        s = matches[0]
                        selected_paths.append(s['path'])
                        active_session_state['stems'].append({'track_idx': track_cursor, 'role': q['role'], 'path': s['path'], 'is_midi': False})
                        vst_client.send_message('/neural_lab/stem_selected', [track_cursor, q['role'], s['filename'], float(s['similarity'])])
                        ableton_client.send_message('/live/song/create_audio_track', [-1])
                        ableton_client.send_message('/live/clip_slot/create_clip', [track_cursor, 0, 16.0])
                        ableton_client.send_message('/live/clip/set/file_path', [track_cursor, 0, s['path']])
                        ableton_client.send_message('/live/clip/fire', [track_cursor, 0])
                        track_cursor += 1

                bass_notes = midi_composer.generate_rolling_bass(key=key, bars=4)
                write_midi_clip_to_ableton(track_cursor, 0, f'NL BASS ({key})', bass_notes, 16.0, create_track=True)
                ableton_client.send_message('/live/clip/fire', [track_cursor, 0])
                active_session_state['stems'].append({'track_idx': track_cursor, 'role': 'MIDI BASS', 'is_midi': True})
                vst_client.send_message('/neural_lab/stem_selected', [track_cursor, 'MIDI BASS', f'Rolling_{key}_Vel.mid', 0.99])
                track_cursor += 1

                arp_notes = midi_composer.generate_hypnotic_arp(key=key, bars=4)
                write_midi_clip_to_ableton(track_cursor, 0, f'NL ARP ({key})', arp_notes, 16.0, create_track=True)
                ableton_client.send_message('/live/clip/fire', [track_cursor, 0])
                active_session_state['stems'].append({'track_idx': track_cursor, 'role': 'MIDI ARP', 'is_midi': True})
                vst_client.send_message('/neural_lab/stem_selected', [track_cursor, 'MIDI ARP', f'Hypnotic_{key}_Auto.mid', 0.99])

                if mode == 'arrange_full':
                    send_status('BUILDING 6-SCENE CLUB ARRANGEMENT...')
                    build_full_club_arrangement(key, bpm)

            dsp_params = mix_engine.compute_dsp_profile(genre, target_lufs, float(mix_target.get('punch', 0.5)), selected_paths)
            vst_client.send_message('/neural_lab/set_dsp', [dsp_params['lowcut'], dsp_params['punch'], dsp_params['width'], dsp_params['drive'], dsp_params['ceiling']])
            send_status(f'ACTIVE: {mode.upper()} ({key} @ {int(bpm)} BPM)')
        except Exception as e:
            traceback.print_exc()
            send_status(f'CMD ERR: {str(e)[:28]}')
    threading.Thread(target=worker, daemon=True).start()

if __name__ == '__main__':
    disp = dispatcher.Dispatcher()
    disp.map('/neural_lab/command', handle_plugin_command)
    disp.map('/neural_lab/add_folder', handle_add_folder)
    disp.map('/neural_lab/set_reference', handle_set_reference)
    disp.map('/neural_lab/swap_stem', handle_swap_stem)
    server = osc_server.ThreadingOSCUDPServer(('127.0.0.1', 9001), disp)
    print('NEURAL LAB v1.1 ENGINE READY ON 127.0.0.1:9001', flush=True)
    server.serve_forever()