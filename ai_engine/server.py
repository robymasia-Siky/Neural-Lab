from ableton_mcp_agent import run_ableton_mcp_agent, is_mcp_enabled, set_mcp_enabled
import os
import sys
import time
import json
import socket
import subprocess
import threading
import traceback
from skill_router import NeuralSkillRouter
from taste_memory import StudioTasteMemory
taste_memory = StudioTasteMemory(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'memory', 'engineer_log.jsonl'))
skill_router = NeuralSkillRouter(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'skills'))
import struct, tempfile

def export_midi_to_temp_file(filename, notes, bpm=126):
    try:
        out_dir = os.path.join(tempfile.gettempdir(), 'NeuralLab_Stems')
        os.makedirs(out_dir, exist_ok=True)
        out_path = os.path.join(out_dir, filename)
        tpq = 480
        tempo_us = int(60_000_000 / max(40, int(bpm)))
        events = []
        events.append((0, b'\xFF\x51\x03' + tempo_us.to_bytes(3, 'big')))
        for n in notes:
            pitch = max(0, min(127, int(n.get('pitch', 60))))
            vel = max(1, min(127, int(n.get('velocity', 100))))
            start_tick = max(0, int(round(float(n.get('start_time', 0.0)) * tpq)))
            dur_tick = max(1, int(round(float(n.get('duration', 0.25)) * tpq)))
            events.append((start_tick, bytes([0x90, pitch, vel])))
            events.append((start_tick + dur_tick, bytes([0x80, pitch, 0])))
        events.sort(key=lambda x: (x[0], 0 if x[1][0] == 0x80 else 1))
        track_data = bytearray()
        last_tick = 0
        for tick, msg in events:
            delta = max(0, tick - last_tick)
            last_tick = tick
            buf = [delta & 0x7F]
            while delta >>= 7:
                buf.append((delta & 0x7F) | 0x80)
            track_data.extend(reversed(buf))
            track_data.extend(msg)
        track_data.extend(b'\x00\xFF\x2F\x00')
        header = b'MThd' + struct.pack('>IHHH', 6, 0, 1, tpq)
        track = b'MTrk' + struct.pack('>I', len(track_data)) + bytes(track_data)
        with open(out_path, 'wb') as f:
            f.write(header + track)
        return out_path
    except Exception:
        return ''
import urllib.request
import numpy as np
from pythonosc import dispatcher, osc_server, udp_client
vst_client = udp_client.SimpleUDPClient('127.0.0.1', 9002)
ableton_client = udp_client.SimpleUDPClient('127.0.0.1', 11000)
vst_client.send_message('/neural_lab/status', ['LOADING NEURAL MODELS...'])

try:
    from install_ableton_osc import ensure_ableton_osc_installed
    ensure_ableton_osc_installed()
except Exception as e:
    print(f"[AbletonOSC init warning] {e}", flush=True)

from orchestrator import NeuralOrchestrator
from midi_composer import NeuralMidiComposer
SampleLibraryIndexer = None
NeuralMixMasterEngine = None

def _lazy_load_heavy_engines():
    global SampleLibraryIndexer, NeuralMixMasterEngine
    try:
        from audio_indexer import SampleLibraryIndexer as _SLI
        SampleLibraryIndexer = _SLI
    except Exception as e:
        print(f"[LazyLoad audio_indexer err] {e}", flush=True)
    try:
        from auto_master import NeuralMixMasterEngine as _NME
        NeuralMixMasterEngine = _NME
    except Exception as e:
        print(f"[LazyLoad auto_master err] {e}", flush=True)

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
CUSTOM_MODEL_NAME = 'neural-lab-0.5b:latest'

def send_status(text: str):
    print(f'[STATUS] {text}', flush=True)
    vst_client.send_message('/neural_lab/status', [str(text)])

def send_chat(text: str, is_error: bool = False):
    prefix = '[ERR] ' if is_error else '[AI] '
    print(f'{prefix}{text}', flush=True)
    vst_client.send_message('/neural_lab/chat', [str(text), 1 if is_error else 0])


def _ensure_custom_gguf_downloaded():
    import urllib.request
    gguf_path = os.path.join(PROJECT_ROOT, 'training', 'neural-lab-0.5b.gguf')
    mf_path = os.path.join(PROJECT_ROOT, 'training', 'Modelfile')
    if not os.path.exists(gguf_path):
        os.makedirs(os.path.dirname(gguf_path), exist_ok=True)
        url = 'https://github.com/robymasia-Siky/Neural-Lab/releases/latest/download/neural-lab-0.5b.gguf'
        try:
            send_chat('Primo avvio: download del modello neurale NEURAL-LAB (948 MB) in corso...')
            urllib.request.urlretrieve(url, gguf_path)
        except Exception:
            return False
    if os.path.exists(gguf_path) and os.path.exists(mf_path):
        _ensure_custom_gguf_downloaded()
        subprocess.run(['ollama', 'create', 'neural-lab-0.5b:latest', '-f', mf_path], check=False)
        return True
    return False

def ensure_ollama_and_model_ready():
    ollama_url = 'http://127.0.0.1:11434/api/tags'
    is_running = False
    try:
        with urllib.request.urlopen(ollama_url, timeout=1.5) as resp:
            if resp.status == 200:
                is_running = True
    except Exception as warmup_err:
        is_running = False

    if not is_running:
        send_status('BOOTING OLLAMA ENGINE...')
        send_chat('Avvio il motore locale Ollama in background...')
        creationflags = 0x08000000 if os.name == 'nt' else 0
        try:
            subprocess.Popen(['ollama', 'serve'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=creationflags)
            for _ in range(15):
                time.sleep(0.5)
                try:
                    with urllib.request.urlopen(ollama_url, timeout=1.0) as resp:
                        if resp.status == 200:
                            is_running = True
                            break
                except Exception as warmup_err:
                    continue
        except Exception as e:
            send_chat(f'Errore avvio Ollama: {str(e)[:45]}', is_error=True)

    try:
        with urllib.request.urlopen(ollama_url, timeout=2.0) as resp:
            tags_data = json.loads(resp.read().decode('utf-8'))
            models = [m.get('name', '') for m in tags_data.get('models', [])]
            if not any(CUSTOM_MODEL_NAME in m for m in models):
                modelfile = os.path.join(PROJECT_ROOT, 'training', 'Modelfile')
                if os.path.exists(modelfile):
                    send_chat('Configurazione modello NEURAL-LAB...')
                    _ensure_custom_gguf_downloaded()
        subprocess.run(['ollama', 'create', CUSTOM_MODEL_NAME, '-f', modelfile], check=False)
    except Exception as warmup_err:
        pass

    try:
        warmup_payload = json.dumps({
            'model': CUSTOM_MODEL_NAME,
            'prompt': 'warmup',
            'stream': False,
            'keep_alive': -1,
            'options': {'num_predict': 1}
        }).encode('utf-8')
        req = urllib.request.Request('http://127.0.0.1:11434/api/generate', data=warmup_payload, headers={'Content-Type': 'application/json'})
        urllib.request.urlopen(req, timeout=30)
        send_status('NEURAL-LAB ENGINE: ONLINE')
        send_chat('Pronto! Motore NEURAL-LAB attivo. Dimmi cosa produrre.')
    except Exception as warmup_err:
        send_status('NEURAL-LAB ENGINE: READY')
        send_chat('Motore NEURAL-LAB avviato in modalita standard.', is_error=True)

orchestrator = NeuralOrchestrator(model_name=CUSTOM_MODEL_NAME, fallback_model='qwen2.5:0.5b')
class _LazyProxy:
    def __init__(self, getter):
        self._getter = getter
        self._inst = None
    def __getattr__(self, name):
        if self._inst is None:
            for _ in range(40):
                cls = self._getter()
                if cls is not None:
                    self._inst = cls()
                    break
                time.sleep(0.5)
        return getattr(self._inst, name)

indexer = _LazyProxy(lambda: SampleLibraryIndexer)
mix_engine = _LazyProxy(lambda: NeuralMixMasterEngine)
midi_composer = NeuralMidiComposer()

current_reference_profile = None
active_session_state = {'key': 'Fm', 'bpm': 126.0, 'stems': [], 'swap_offsets': {}}

def load_audio_fast(filepath: str, max_seconds: int = 30):
    try:
        import soundfile as sf
        info = sf.info(filepath)
        sr = info.samplerate
        data, _ = sf.read(filepath, frames=int(sr * max_seconds), dtype='float32', always_2d=True)
        return np.mean(data, axis=1), sr
    except Exception as warmup_err:
        import librosa
        y, sr = librosa.load(filepath, sr=22050, duration=float(max_seconds), mono=True)
        return y, sr

def handle_add_folder(address, folder_path):
    def worker():
        try:
            folder_name = os.path.basename(folder_path)
            send_status(f'CLAP: INDEXING {folder_name}...')
            send_chat(f'Ok, indicizzo i sample nella cartella "{folder_name}" con CLAP...')
            total = indexer.add_folder_incremental(folder_path)
            if total == 0:
                send_status('LIBRARY: 0 SAMPLES')
                send_chat('Attenzione: nessun file audio valido trovato in questa cartella.', is_error=True)
            else:
                send_status(f'LIBRARY READY: {total} SAMPLES')
                send_chat(f'Perfetto! Libreria aggiornata: {total} sample pronti per la ricerca.')
        except Exception as e:
            traceback.print_exc()
            send_status('INDEX ERROR')
            send_chat(f'Errore indicizzazione sample: {str(e)[:50]}', is_error=True)
    threading.Thread(target=worker, daemon=True).start()

def handle_set_reference(address, ref_path):
    def worker():
        global current_reference_profile
        try:
            clean_path = os.path.normpath(str(ref_path).strip().strip('"'))
            if not os.path.exists(clean_path):
                send_status('REF ERR: NOT FOUND')
                send_chat('Errore: file reference non trovato sul disco.', is_error=True)
                return
            fname = os.path.basename(clean_path)
            send_status('MATCHERING: EXTRACTING EQ...')
            send_chat(f'Ok, analizzo spettro EQ e loudness di "{fname}"...')
            y, sr = load_audio_fast(clean_path, max_seconds=30)
            if y is None or len(y) < 2048:
                send_status('REF ERR: TOO SHORT')
                send_chat('Errore: il file reference e troppo corto per l analisi.', is_error=True)
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
                specs = [np.abs(np.fft.rfft(y[idx * hop:idx * hop + n_fft] * win)) for idx in range(0, num_frames, step_stride)]
            mean_spec = np.mean(specs, axis=0)
            max_val = float(np.max(mean_spec)) + 1e-8
            db_spec = 20.0 * np.log10(mean_spec / max_val + 1e-8)
            curve = [float(np.clip((db_spec[min(int(1.09 ** i), len(db_spec) - 1)] + 55.0) / 55.0, 0.08, 0.92)) for i in range(64)]
            current_reference_profile = {'target_lufs': ref_lufs, 'path': clean_path, 'curve': curve}
            vst_client.send_message('/neural_lab/ref_curve', [float(v) for v in curve])
            send_status(f'REF LOCKED: {round(ref_lufs, 1)} LUFS')
            send_chat(f'Curva Reference agganciata! Target impostato a {round(ref_lufs, 1)} LUFS.')
        except Exception as exc:
            traceback.print_exc()
            send_status('REF ERROR')
            send_chat(f'Errore analisi reference: {str(exc)[:48]}', is_error=True)
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
                    write_midi_clip_to_ableton(t_idx, s_idx, f"{stem['role']} ({sec_name[:7]})", notes, 16.0, create_track=False)
                elif stem.get('path'):
                    ableton_client.send_message('/live/clip_slot/create_clip', [t_idx, s_idx, 16.0])
                    ableton_client.send_message('/live/clip/set/file_path', [t_idx, s_idx, stem['path']])
    ableton_client.send_message('/live/scene/fire', [2])


def handle_audition_stem(address, stem_idx, solo_state):
    def worker():
        try:
            solo_flag = 1 if int(solo_state) != 0 else 0
            # Resetta eventuali altri solo prima di attivare quello richiesto
            for idx in range(8):
                ableton_client.send_message('/live/track/set/solo', [idx, 0])
            if solo_flag == 1:
                ableton_client.send_message('/live/track/set/solo', [int(stem_idx), 1])
                ableton_client.send_message('/live/clip/fire', [int(stem_idx), 0])
                send_status(f'AUDITION: TRACK #{int(stem_idx)+1} SOLO')
            else:
                send_status('NEURAL-LAB ENGINE: ONLINE')
        except Exception as e:
            traceback.print_exc()
    threading.Thread(target=worker, daemon=True).start()

def handle_lock_stem(address, stem_idx, is_locked):
    idx = int(stem_idx)
    locked = (int(is_locked) != 0)
    if 0 <= idx < len(active_session_state.get('stems', [])):
        active_session_state['stems'][idx]['locked'] = locked
        role = active_session_state['stems'][idx].get('role', f'Track {idx+1}')
        send_chat(f'Stem {role} {"bloccato (LOCKED)" if locked else "sbloccato"}.')
def handle_swap_stem(address, stem_idx, role):
    def worker():
        try:
            offset = active_session_state['swap_offsets'].get(stem_idx, 0) + 1
            active_session_state['swap_offsets'][stem_idx] = offset
            key = active_session_state['key']
            send_chat(f'Ok, cerco un alternativa per lo stem {role}...')
            if 'MIDI' in role.upper():
                notes = midi_composer.generate_rolling_bass(key, 4) if 'BASS' in role.upper() else midi_composer.generate_hypnotic_arp(key, 4)
                clean_role = role.replace('MIDI ', '')
                new_name = f"{clean_role}_{key}_Var{offset}.mid"
                write_midi_clip_to_ableton(stem_idx, 0, new_name, notes, 16.0, create_track=False)
                ableton_client.send_message('/live/clip/fire', [stem_idx, 0])
                mid_path = export_midi_to_temp_file(new_name, notes)
                vst_client.send_message('/neural_lab/stem_selected', [stem_idx, role, new_name, 0.99, mid_path])
                send_status(f'SWAPPED {role}: VAR #{offset}')
                send_chat(f'Nuova variazione MIDI generata per {role} (#{offset}).')
            else:
                query = f'dark melodic techno {role.lower()}'
                matches = indexer.search_samples(query, top_k=offset + 2)
                if len(matches) > offset:
                    sample = matches[offset]
                    ableton_client.send_message('/live/clip/set/file_path', [stem_idx, 0, sample['path']])
                    ableton_client.send_message('/live/clip/fire', [stem_idx, 0])
                    vst_client.send_message('/neural_lab/stem_selected', [stem_idx, role, sample['filename'], float(sample['similarity']), sample['path']])
                    send_status(f"SWAPPED {role}")
                    send_chat(f"Stem {role} sostituito con {sample['filename']}.")
                else:
                    send_chat('Attenzione: nessun altro sample disponibile. Aggiungi altre cartelle!', is_error=True)
        except Exception as e:
            traceback.print_exc()
            send_chat(f'Errore cambio stem: {str(e)[:48]}', is_error=True)
    threading.Thread(target=worker, daemon=True).start()

def handle_plugin_command(address, mode, prompt_text, ref_path=''):
    def worker():
        try:
            if mode == 'auto_master':
                send_chat('Ok, eseguo mix e auto-mastering sul bus principale...')
            elif mode == 'arrange_full':
                send_chat('Ok, creo la traccia e costruisco l arrangiamento completo a 6 scene...')
            else:
                send_chat('Ok, analizzo il prompt con NEURAL-LAB e preparo gli stems...')

            send_status('NEURAL-LAB: PROCESSING...')
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
                prev_stems = active_session_state.get('stems', [])
                locked_roles = {s['role']: s for s in prev_stems if s.get('locked', False)}
                ableton_client.send_message('/live/song/set/tempo', [bpm])

                if not locked_roles:
                    active_session_state['stems'] = []
                    active_session_state['swap_offsets'] = {}
                    track_cursor = 0
                else:
                    track_cursor = len(prev_stems)

                queries = plan.get('sample_queries', [])[:2]
                found_any_sample = False

                for q in queries:
                    if q['role'] in locked_roles:
                        if locked_roles[q['role']].get('path'):
                            selected_paths.append(locked_roles[q['role']]['path'])
                        continue
                    matches = indexer.search_samples(q['query'], top_k=1, sample_type=q.get('type'))
                    if matches:
                        found_any_sample = True
                        s = matches[0]
                        selected_paths.append(s['path'])
                        existing_idx = next((i for i, st in enumerate(active_session_state['stems']) if st['role'] == q['role'] and not st.get('locked')), None)
                        target_idx = existing_idx if existing_idx is not None else track_cursor
                        create_new = (existing_idx is None)
                        stem_obj = {'track_idx': target_idx, 'role': q['role'], 'path': s['path'], 'is_midi': False, 'locked': False}
                        if create_new:
                            active_session_state['stems'].append(stem_obj)
                            ableton_client.send_message('/live/song/create_audio_track', [-1])
                            track_cursor += 1
                        else:
                            active_session_state['stems'][target_idx] = stem_obj
                        vst_client.send_message('/neural_lab/stem_selected', [target_idx, q['role'], s['filename'], float(s['similarity']), s['path']])
                        ableton_client.send_message('/live/clip_slot/create_clip', [target_idx, 0, 16.0])
                        ableton_client.send_message('/live/clip/set/file_path', [target_idx, 0, s['path']])
                        ableton_client.send_message('/live/clip/fire', [target_idx, 0])

                if not found_any_sample and not locked_roles:
                    send_chat('Nota: libreria sample vuota, genero le tracce MIDI su Ableton.', is_error=True)

                if 'MIDI BASS' not in locked_roles:
                    existing_bass = next((i for i, st in enumerate(active_session_state['stems']) if st['role'] == 'MIDI BASS' and not st.get('locked')), None)
                    b_idx = existing_bass if existing_bass is not None else track_cursor
                    c_bass = (existing_bass is None)
                    bass_notes = midi_composer.generate_rolling_bass(key=key, bars=4)
                    write_midi_clip_to_ableton(b_idx, 0, f'NL BASS ({key})', bass_notes, 16.0, create_track=c_bass)
                    ableton_client.send_message('/live/clip/fire', [b_idx, 0])
                    b_obj = {'track_idx': b_idx, 'role': 'MIDI BASS', 'is_midi': True, 'locked': False}
                    if c_bass:
                        active_session_state['stems'].append(b_obj)
                        track_cursor += 1
                    else:
                        active_session_state['stems'][b_idx] = b_obj
                    b_mid_name = f'Rolling_{key}_Vel.mid'
                    b_mid_path = export_midi_to_temp_file(b_mid_name, bass_notes, bpm)
                    vst_client.send_message('/neural_lab/stem_selected', [b_idx, 'MIDI BASS', b_mid_name, 0.99, b_mid_path])

                if 'MIDI ARP' not in locked_roles:
                    existing_arp = next((i for i, st in enumerate(active_session_state['stems']) if st['role'] == 'MIDI ARP' and not st.get('locked')), None)
                    a_idx = existing_arp if existing_arp is not None else track_cursor
                    c_arp = (existing_arp is None)
                    arp_notes = midi_composer.generate_hypnotic_arp(key=key, bars=4)
                    write_midi_clip_to_ableton(a_idx, 0, f'NL ARP ({key})', arp_notes, 16.0, create_track=c_arp)
                    ableton_client.send_message('/live/clip/fire', [a_idx, 0])
                    a_obj = {'track_idx': a_idx, 'role': 'MIDI ARP', 'is_midi': True, 'locked': False}
                    if c_arp:
                        active_session_state['stems'].append(a_obj)
                        track_cursor += 1
                    else:
                        active_session_state['stems'][a_idx] = a_obj
                    a_mid_name = f'Hypnotic_{key}_Auto.mid'
                    a_mid_path = export_midi_to_temp_file(a_mid_name, arp_notes, bpm)
                    vst_client.send_message('/neural_lab/stem_selected', [a_idx, 'MIDI ARP', a_mid_name, 0.99, a_mid_path])

                if mode == 'arrange_full':
                    send_status('BUILDING 6-SCENE ARRANGEMENT...')
                    build_full_club_arrangement(key, bpm)

            dsp_params = mix_engine.compute_dsp_profile(genre, target_lufs, float(mix_target.get('punch', 0.5)), selected_paths)
            taste_memory.record_decision(genre, ref_path, {'lowcut': dsp_params[0], 'punch': dsp_params[1], 'width': dsp_params[2], 'drive': dsp_params[3], 'ceiling': dsp_params[4]}, 'auto_master')
            vst_client.send_message('/neural_lab/set_dsp', [dsp_params['lowcut'], dsp_params['punch'], dsp_params['width'], dsp_params['drive'], dsp_params['ceiling']])
            send_status(f'ACTIVE: {mode.upper()} ({key} @ {int(bpm)} BPM)')

            if mode == 'auto_master':
                send_chat(f'Mastering completato! Target {round(target_lufs, 1)} LUFS, Punch {int(dsp_params["punch"]*100)}%, Width {int(dsp_params["width"]*100)}%.')
            elif mode == 'arrange_full':
                send_chat(f'Arrangiamento completato in {key} a {int(bpm)} BPM! 6 scene pronte su Ableton.')
            else:
                send_chat(f'Fatto! Base in {key} a {int(bpm)} BPM inviata ad Ableton e DSP calibrato.')

        except Exception as e:
            traceback.print_exc()
            send_status('EXECUTION ERROR')
            send_chat(f'Errore esecuzione comando: {str(e)[:52]}', is_error=True)
    threading.Thread(target=worker, daemon=True).start()


import re

def extract_musical_intent(prompt: str):
    p = prompt.lower()
    bpm_match = re.search(r'\b(1[0-5][0-9]|[89][0-9])\s*bpm\b', p)
    bpm = float(bpm_match.group(1)) if bpm_match else None

    key_match = re.search(r'\b([a-g](?:#|b)?m?)\b', prompt, re.IGNORECASE)
    key = None
    if key_match:
        cand = key_match.group(1)
        if cand.lower() not in ['a', 'e', 'in', 'di', 'un', 'il', 'la', 'su', 'per', 'con', 'fa', 'do', 're', 'mi', 'sol', 'si']:
            key = cand[0].upper() + cand[1:]
        else:
            ita_map = {'do': 'C', 're': 'D', 'mi': 'E', 'fa': 'F', 'sol': 'G', 'la': 'A', 'si': 'B'}
            if cand.lower() in ita_map:
                key = ita_map[cand.lower()] + ('m' if 'minor' in p or 'minore' in p else '')

    is_midi_req = any(k in p for k in ['midi', 'basso', 'bass', 'arp', 'arpeggio', 'pad', 'accordi', 'chords', 'melodia', 'melody', 'lead', 'linea di'])
    is_create_req = any(k in p for k in ['crea', 'genera', 'aggiungi', 'scrivi', 'fammi', 'metti', 'create', 'generate', 'add', 'make'])
    return is_midi_req and is_create_req, key, bpm

def infer_and_send_dsp_suggestion(question: str, reply: str):
    combined = f"{question} {reply}".lower()
    preset_name = "CLUB MASTER BALANCED"
    lowcut, punch, width, drive, ceiling, eqmatch = 30.0, 0.42, 1.08, 1.2, -0.3, 60.0

    if any(k in combined for k in ["loud", "forte", "sping", "club", "peak", "hard", "rave", "lufs"]):
        preset_name = "CLUB PEAK -7 LUFS"
        lowcut, punch, width, drive, ceiling, eqmatch = 32.0, 0.68, 1.12, 2.8, -0.2, 75.0
    elif any(k in combined for k in ["sub", "bass", "basso", "fango", "mud", "kick", "cassa", "250hz"]):
        preset_name = "TIGHT SUB & KICK"
        lowcut, punch, width, drive, ceiling, eqmatch = 36.0, 0.55, 1.00, 1.5, -0.3, 65.0
    elif any(k in combined for k in ["wide", "stereo", "spazio", "aria", "air", "atmosfer", "melodic", "pad"]):
        preset_name = "HYPNOTIC WIDE AIR"
        lowcut, punch, width, drive, ceiling, eqmatch = 28.0, 0.38, 1.28, 1.0, -0.4, 70.0
    elif any(k in combined for k in ["warm", "caldo", "analog", "satur", "drive", "distor"]):
        preset_name = "ANALOG WARMTH"
        lowcut, punch, width, drive, ceiling, eqmatch = 28.0, 0.58, 1.06, 3.4, -0.3, 55.0

    vst_client.send_message('/neural_lab/suggest_dsp', [
        preset_name, float(lowcut), float(punch), float(width), float(drive), float(ceiling), float(eqmatch)
    ])

def emit_dsp_suggestion_for_prompt(question: str, reply: str):
    q = (question + " " + reply).lower()
    # [label, lowcut_hz, punch_0_1, width_0_2, drive_db, eqmatch_pct, ceiling_db]
    if any(k in q for k in ['techno', 'rave', 'hard', 'club', 'peak', 'acid', 'cox']):
        preset = ['CLUB TECHNO PUNCH', 32.0, 0.62, 1.18, 2.4, 65.0, -0.2]
    elif any(k in q for k in ['deep', 'house', 'warm', 'analog', 'groove', 'soul']):
        preset = ['DEEP ANALOG WARMTH', 28.0, 0.38, 1.12, 1.8, 50.0, -0.4]
    elif any(k in q for k in ['wide', 'stereo', 'spazio', 'aria', 'air', 'reverb', 'pad']):
        preset = ['HYPNOTIC WIDE AIR', 34.0, 0.42, 1.42, 1.0, 55.0, -0.3]
    elif any(k in q for k in ['sub', 'basso', 'bass', 'kick', 'fango', 'mud', '250', 'low']):
        preset = ['TIGHT SUB & KICK', 38.0, 0.54, 1.02, 1.2, 60.0, -0.3]
    elif any(k in q for k in ['master', 'lufs', 'volume', 'loud', 'limiter', 'final']):
        preset = ['PRO CLUB MASTER', 30.0, 0.58, 1.15, 3.0, 75.0, -0.1]
    else:
        preset = ['BALANCED STUDIO MIX', 30.0, 0.45, 1.10, 1.5, 55.0, -0.3]
    vst_client.send_message('/neural_lab/dsp_suggestion', preset)

def query_studio_advisor(question: str, telemetry_str: str = '') -> str:
    active_skill = skill_router.route(question, prefer_mcp=False)
    skill_context = (active_skill['body'] if active_skill else "") + ("\n" + taste_memory.get_recent_taste_summary() if taste_memory.get_recent_taste_summary() else "")
    if active_skill and active_skill.get('dsp_preset'):
        vst_client.send_message('/neural_lab/suggest_dsp', active_skill['dsp_preset'])
    sys_prompt = (
        "Sei NEURAL LAB Copilot, un ingegnere del suono senior. "
        "Applica rigorosamente questa Skill tecnica:\n" + skill_context + "\n"
        (f"\nTELEMETRIA AUDIO REALE DAL PLUGIN:\n{telemetry_str}\n" if telemetry_str else "") + "Usa i dati reali della telemetria (LUFS, GR, bande, DSP attuali) se presenti. Rispondi in italiano in massimo 2 frasi brevi, concrete e pratiche (max 180 caratteri totali), senza JSON."
    )
    payload = json.dumps({
        'model': CUSTOM_MODEL_NAME,
        'system': sys_prompt,
        'prompt': question,
        'stream': False,
        'keep_alive': -1,
        'options': {'temperature': 0.35, 'num_predict': 90}
    }).encode('utf-8')
    req = urllib.request.Request('http://127.0.0.1:11434/api/generate', data=payload, headers={'Content-Type': 'application/json'})
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            ans = data.get('response', '').strip().replace('\n', ' ')
            if ans.startswith('{') or not ans:
                return "Taglia sotto i 30Hz con filtro ripido, scava 2-3dB sui 250Hz per pulire il fango e lascia spazio al kick."
            return ans[:175]
    except Exception:
        return "Per un mix pulito: high-pass a 30Hz sul master, sidechain leggero sul basso e controllo medio-bassi a 250Hz."


def handle_set_mcp_enabled(address, enabled_flag):
    def worker():
        state = (int(enabled_flag) != 0)
        send_status('MCP AGENT: LOADING VRAM...' if state else 'MCP AGENT: UNLOADING VRAM...')
        set_mcp_enabled(state)
        vst_client.send_message('/neural_lab/mcp_state', [1 if state else 0])
        send_status('MCP AGENT: ACTIVE' if state else 'MCP AGENT: DISABLED')
        send_chat('Ableton MCP + Qwen Coder attivato e caricato in VRAM.' if state else 'Ableton MCP disattivato (VRAM liberata).')
    threading.Thread(target=worker, daemon=True).start()
def handle_ask_chat(address, *args):
    def worker():
        try:
            text = str(args[0]).strip() if len(args) >= 1 else ''
            telemetry_str = str(args[1]).strip() if len(args) >= 2 else ''
            if not text:
                return
            is_midi, parsed_key, parsed_bpm = extract_musical_intent(text)
            # Se il prompt chiede azioni su Ableton (tracce, arrangiamento, velocity, envelope, synth, mixer, sessione) usa l'agente MCP con qwen2.5-coder:3b
            mcp_keywords = ['ableton', 'traccia', 'track', 'velocity', 'envelope', 'automazione', 'automation',
                            'arrangiamento', 'arrangement', 'synth', 'wavetable', 'operator', 'drift', 'effett',
                            'reverb', 'delay', 'filter', 'cutoff', 'volume', 'pan', 'sessione', 'session',
                            'cue', 'marker', 'loop', 'clip', 'device', 'carica', 'load', 'modifica', 'umanizza', 'hardware', 'cc', 'program', 'arm', 'timeline', 'view', 'cursore', 'scala', 'accordi', 'pattern']
            if is_mcp_enabled() and any(k in text.lower() for k in mcp_keywords):
                send_status('MCP AGENT: ANALYZING...')
                ctx = f"Key={active_session_state.get('key', 'Fm')}, BPM={active_session_state.get('bpm', 126)} | {telemetry_str}"
                reply = run_ableton_mcp_agent(text, session_context=ctx, status_callback=send_status)
                send_chat(reply)
                send_status('NEURAL-LAB ENGINE: ONLINE')
                return

            if is_midi:
                key = parsed_key or active_session_state.get('key', 'Fm')
                bpm = parsed_bpm or active_session_state.get('bpm', 126.0)
                active_session_state['key'] = key
                active_session_state['bpm'] = bpm
                if parsed_bpm:
                    ableton_client.send_message('/live/song/set/tempo', [bpm])

                p_low = text.lower()
                if any(w in p_low for w in ['pad', 'accordi', 'chords']):
                    role_name = f"NL PAD ({key})"
                    notes = midi_composer.generate_pad_chords(key=key, bars=4)
                elif any(w in p_low for w in ['arp', 'arpeggio', 'melodia', 'melody', 'lead']):
                    role_name = f"NL ARP ({key})"
                    notes = midi_composer.generate_hypnotic_arp(key=key, bars=4)
                else:
                    role_name = f"NL BASS ({key})"
                    notes = midi_composer.generate_rolling_bass(key=key, bars=4)

                send_status(f'CREATING MIDI: {role_name}')
                send_chat(f'Ok, genero "{role_name}" a {int(bpm)} BPM su Ableton Live...')

                next_track_idx = len(active_session_state.get('stems', []))
                write_midi_clip_to_ableton(next_track_idx, 0, role_name, notes, 16.0, create_track=True)
                ableton_client.send_message('/live/clip/fire', [next_track_idx, 0])

                active_session_state['stems'].append({'track_idx': next_track_idx, 'role': role_name, 'is_midi': True})
                m_name = f"{role_name.replace(' ', '_')}.mid"
                m_path = export_midi_to_temp_file(m_name, notes)
                vst_client.send_message('/neural_lab/stem_selected', [next_track_idx, role_name, m_name, 0.99, m_path])
                send_status(f'MIDI CREATED: {role_name}')
                send_chat(f'Fatto! Traccia MIDI "{role_name}" creata e avviata su Ableton.')
            else:
                send_status('AI COPILOT: THINKING...')
                reply = query_studio_advisor(text, telemetry_str)
                emit_dsp_suggestion_for_prompt(text, reply)
                infer_and_send_dsp_suggestion(text, reply)
                send_status('NEURAL-LAB ENGINE: ONLINE')
                send_chat(reply)
        except Exception as e:
            traceback.print_exc()
            send_status('CHAT ERROR')
            send_chat(f'Errore elaborazione richiesta: {str(e)[:48]}', is_error=True)
    threading.Thread(target=worker, daemon=True).start()
if __name__ == '__main__':
    threading.Thread(target=_lazy_load_heavy_engines, daemon=True).start()
    threading.Thread(target=ensure_ollama_and_model_ready, daemon=True).start()
    disp = dispatcher.Dispatcher()
    disp.map('/neural_lab/command', handle_plugin_command)
    disp.map('/neural_lab/add_folder', handle_add_folder)
    disp.map('/neural_lab/set_reference', handle_set_reference)
    disp.map('/neural_lab/swap_stem', handle_swap_stem)
    disp.map('/neural_lab/audition_stem', handle_audition_stem)
    disp.map('/neural_lab/lock_stem', handle_lock_stem)
    def handle_ping(address, *args):
        enabled = is_mcp_enabled()
        vst_client.send_message('/neural_lab/mcp_state', [1 if enabled else 0])
        send_status('NEURAL-LAB ENGINE: ONLINE')

    disp.map('/neural_lab/ping', handle_ping)
    disp.map('/neural_lab/set_mcp_enabled', handle_set_mcp_enabled)
    def init_mcp_on_boot():
        time.sleep(1.0)
        enabled = is_mcp_enabled()
        vst_client.send_message('/neural_lab/mcp_state', [1 if enabled else 0])
        send_status('NEURAL-LAB ENGINE: ONLINE')
        time.sleep(3.5)
        if enabled:
            set_mcp_enabled(True)
    threading.Thread(target=init_mcp_on_boot, daemon=True).start()
    def handle_dsp_applied(address, *args):
    try:
        if len(args) >= 6:
            preset_name = str(args[0])
            dsp_vals = {
                'lowcut': round(float(args[1]), 1),
                'punch': round(float(args[2]), 2),
                'width': round(float(args[3]), 2),
                'drive': round(float(args[4]), 1),
                'ceiling': round(float(args[5]), 1),
            }
            ref_name = str(args[6]) if len(args) >= 7 else ''
            genre = active_session_state.get('genre', 'electronic')
            taste_memory.record_decision(genre, ref_name, dsp_vals, f"user_approved:{preset_name}")
            print(f"[TASTE MEMORY] Saved approved preset: {preset_name} -> {dsp_vals}", flush=True)
    except Exception:
        traceback.print_exc()
    disp.map('/neural_lab/dsp_applied', handle_dsp_applied)
    disp.map('/neural_lab/ask', handle_ask_chat)
    osc_server.ThreadingOSCUDPServer.allow_reuse_address = True
    server = osc_server.ThreadingOSCUDPServer(('127.0.0.1', 9001), disp)
    send_status('NEURAL-LAB ENGINE: ONLINE')
    print('NEURAL LAB ENGINE READY ON 127.0.0.1:9001 (MODEL: neural-lab-0.5b)', flush=True)
    server.serve_forever()






