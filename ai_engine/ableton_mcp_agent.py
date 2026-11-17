import json
import os
import random
import socket
import urllib.request

ABLETON_MCP_HOST = "127.0.0.1"
ABLETON_MCP_PORT = 9877
OLLAMA_CHAT_URL = "http://127.0.0.1:11434/api/chat"
AGENT_MODEL = "qwen2.5-coder:3b"
SETTINGS_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "mcp_settings.json")


def is_mcp_enabled() -> bool:
    try:
        if os.path.exists(SETTINGS_FILE):
            with open(SETTINGS_FILE, "r", encoding="utf-8") as f:
                return bool(json.load(f).get("mcp_enabled", True))
    except Exception:
        pass
    return True


def set_mcp_enabled(enabled: bool):
    try:
        with open(SETTINGS_FILE, "w", encoding="utf-8") as f:
            json.dump({"mcp_enabled": bool(enabled)}, f)
    except Exception:
        pass
    try:
        payload = {"model": AGENT_MODEL, "keep_alive": -1 if enabled else 0}
        req = urllib.request.Request(
            "http://127.0.0.1:11434/api/generate",
            data=json.dumps(payload).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        urllib.request.urlopen(req, timeout=15.0).read()
    except Exception:
        pass


def send_mcp_command(command_type: str, params: dict = None) -> dict:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(8.0)
    try:
        sock.connect((ABLETON_MCP_HOST, ABLETON_MCP_PORT))
        payload = json.dumps({"type": command_type, "params": params or {}}) + "\n"
        sock.sendall(payload.encode("utf-8"))
        chunks = []
        while True:
            data = sock.recv(8192)
            if not data:
                break
            chunks.append(data)
            joined = b"".join(chunks).decode("utf-8", errors="ignore")
            try:
                return json.loads(joined)
            except json.JSONDecodeError:
                continue
        return {"status": "error", "message": "Empty response from AbletonMCP"}
    except Exception as e:
        return {"status": "error", "message": f"AbletonMCP non raggiungibile (porta 9877): {e}"}
    finally:
        try:
            sock.close()
        except Exception:
            pass


def _zb(val: int) -> int:
    return max(0, int(val) - 1)


# Motore teorico musicale avanzato per generare pattern con scale, groove e velocity humanization
SCALES = {
    "C": [0, 2, 4, 5, 7, 9, 11], "Cm": [0, 2, 3, 5, 7, 8, 10],
    "C#m": [1, 3, 4, 6, 8, 9, 11], "Dm": [2, 4, 5, 7, 9, 10, 0],
    "D#m": [3, 5, 6, 8, 10, 11, 1], "Em": [4, 6, 7, 9, 11, 0, 2],
    "F": [5, 7, 9, 10, 0, 2, 4], "Fm": [5, 7, 8, 10, 0, 1, 3],
    "F#m": [6, 8, 9, 11, 1, 2, 4], "Gm": [7, 9, 10, 0, 2, 3, 5],
    "G#m": [8, 10, 11, 1, 3, 4, 6], "Am": [9, 11, 0, 2, 4, 5, 7],
    "A#m": [10, 0, 1, 3, 5, 6, 8], "Bm": [11, 1, 2, 4, 6, 7, 9],
}


def compose_smart_notes(key: str = "Fm", style: str = "rolling_bass", bars: int = 4, intensity: float = 0.8) -> list:
    scale = SCALES.get(key, SCALES["Fm"])
    total_beats = max(1, int(bars)) * 4
    notes = []
    if style == "rolling_bass":
        root_oct = 36 + scale[0]
        fifth_oct = 36 + scale[4]
        octave_up = root_oct + 12
        vel_curve = [48, 86, 118, 92]
        step = 0.25
        t = 0.0
        idx = 0
        while t < total_beats:
            sub = idx % 4
            if sub != 0:  # Lascia respirare il kick sul downbeat
                pitch = octave_up if (idx % 16 == 14) else (fifth_oct if (idx % 8 == 6) else root_oct)
                vel = int(min(127, max(25, vel_curve[sub] * intensity + random.randint(-5, 5))))
                notes.append({"pitch": pitch, "start_time": round(t, 4), "duration": 0.20, "velocity": vel, "mute": False})
            t += step
            idx += 1
    elif style == "arp":
        base = 60
        motif = [0, 2, 4, 6, 4, 2, 1, 4]
        t = 0.0
        idx = 0
        while t < total_beats:
            deg = motif[idx % len(motif)]
            oct_shift = 12 if (idx % 8 in [3, 7]) else 0
            pitch = base + scale[deg % len(scale)] + oct_shift
            vel = int(min(127, max(35, (65 + (idx % 4) * 14) * intensity + random.randint(-6, 6))))
            notes.append({"pitch": pitch, "start_time": round(t, 4), "duration": 0.22, "velocity": vel, "mute": False})
            t += 0.25
            idx += 1
    else:  # chords / pad
        base = 48
        prog = [0, 5, 3, 4]  # i - VI - iv - v
        for b in range(bars):
            deg = prog[b % len(prog)]
            chord_tones = [
                base + scale[deg % 7],
                base + scale[(deg + 2) % 7] + (12 if (deg + 2) >= 7 else 0),
                base + scale[(deg + 4) % 7] + (12 if (deg + 4) >= 7 else 0),
                base + scale[(deg + 6) % 7] + (12 if (deg + 6) >= 7 else 0),
            ]
            for p in chord_tones:
                notes.append({
                    "pitch": p,
                    "start_time": float(b * 4),
                    "duration": 3.85,
                    "velocity": int(min(120, max(45, 82 * intensity + random.randint(-4, 4)))),
                    "mute": False
                })
    return notes


def execute_tool(name: str, args: dict) -> str:
    try:
        if name == "get_session_info":
            res = send_mcp_command("get_session_info")
        elif name == "get_track_info":
            res = send_mcp_command("get_track_info", {"track_index": _zb(args.get("track_index", 1))})
        elif name == "set_tempo":
            res = send_mcp_command("set_tempo", {"tempo": float(args.get("tempo", 126.0))})
        elif name == "create_midi_track":
            res = send_mcp_command("create_midi_track", {"index": int(args.get("index", -1))})
        elif name == "set_track_name":
            res = send_mcp_command("set_track_name", {"track_index": _zb(args.get("track_index", 1)), "name": str(args.get("name", "AI Track"))})
        elif name == "delete_track":
            res = send_mcp_command("delete_track", {"track_index": _zb(args.get("track_index", 1))})
        elif name == "set_track_arm":
            res = send_mcp_command("set_track_arm", {"track_index": _zb(args.get("track_index", 1)), "arm": bool(args.get("arm", True))})
        elif name == "create_clip":
            res = send_mcp_command("create_clip", {"track_index": _zb(args.get("track_index", 1)), "clip_index": _zb(args.get("clip_index", 1)), "length": float(args.get("length", 16.0))})
        elif name == "add_notes_to_clip":
            norm_notes = [{
                "pitch": int(n.get("pitch", 60)),
                "start_time": float(n.get("start_time", 0.0)),
                "duration": float(n.get("duration", 0.25)),
                "velocity": max(1, min(127, int(n.get("velocity", 100)))),
                "mute": bool(n.get("mute", False))
            } for n in args.get("notes", [])]
            res = send_mcp_command("add_notes_to_clip", {"track_index": _zb(args.get("track_index", 1)), "clip_index": _zb(args.get("clip_index", 1)), "notes": norm_notes})
        elif name == "generate_musical_pattern":
            notes = compose_smart_notes(
                key=str(args.get("key", "Fm")),
                style=str(args.get("style", "rolling_bass")),
                bars=int(args.get("bars", 4)),
                intensity=float(args.get("intensity", 0.85))
            )
            res = send_mcp_command("add_notes_to_clip", {
                "track_index": _zb(args.get("track_index", 1)),
                "clip_index": _zb(args.get("clip_index", 1)),
                "notes": notes
            })
        elif name == "set_clip_name":
            res = send_mcp_command("set_clip_name", {"track_index": _zb(args.get("track_index", 1)), "clip_index": _zb(args.get("clip_index", 1)), "name": str(args.get("name", "AI Clip"))})
        elif name == "fire_clip":
            res = send_mcp_command("fire_clip", {"track_index": _zb(args.get("track_index", 1)), "clip_index": _zb(args.get("clip_index", 1))})
        elif name == "stop_clip":
            res = send_mcp_command("stop_clip", {"track_index": _zb(args.get("track_index", 1)), "clip_index": _zb(args.get("clip_index", 1))})
        elif name == "start_playback":
            res = send_mcp_command("start_playback")
        elif name == "stop_playback":
            res = send_mcp_command("stop_playback")
        # Full Arrangement View Timeline Control
        elif name == "get_arrangement_info":
            res = send_mcp_command("get_arrangement_info")
        elif name == "set_view":
            res = send_mcp_command("set_view", {"view_name": str(args.get("view_name", "Arranger"))})
        elif name == "set_song_time":
            res = send_mcp_command("set_song_time", {"time": float(args.get("time", 0.0))})
        elif name == "create_arrangement_clip":
            res = send_mcp_command("create_arrangement_clip", {"track_index": _zb(args.get("track_index", 1)), "start_time": float(args.get("start_time", 0.0)), "length": float(args.get("length", 16.0))})
        elif name == "add_notes_to_arrangement_clip":
            res = send_mcp_command("add_notes_to_arrangement_clip", {"track_index": _zb(args.get("track_index", 1)), "clip_index": _zb(args.get("clip_index", 1)), "notes": args.get("notes", [])})
        elif name == "duplicate_to_arrangement":
            res = send_mcp_command("duplicate_to_arrangement", {"track_index": _zb(args.get("track_index", 1)), "clip_index": _zb(args.get("clip_index", 1)), "start_time": float(args.get("start_time", 0.0))})
        elif name == "delete_arrangement_clip":
            res = send_mcp_command("delete_arrangement_clip", {"track_index": _zb(args.get("track_index", 1)), "clip_index": _zb(args.get("clip_index", 1))})
        elif name == "set_arrangement_clip_property":
            res = send_mcp_command("set_arrangement_clip_property", {
                "track_index": _zb(args.get("track_index", 1)),
                "clip_index": _zb(args.get("clip_index", 1)),
                "property": str(args.get("property", "looping")),
                "value": args.get("value", True)
            })
        elif name == "set_arrangement_loop":
            res = send_mcp_command("set_arrangement_loop", {"start": float(args.get("start", 0.0)), "length": float(args.get("length", 64.0)), "enabled": bool(args.get("enabled", True))})
        elif name == "create_cue_point":
            res = send_mcp_command("create_cue_point", {"time": float(args.get("time", 0.0)), "name": str(args.get("name", "Section"))})
        elif name == "jump_to_cue":
            res = send_mcp_command("jump_to_cue", {"index": _zb(args.get("index", 1))})
        # Automation Point Placement (bug-free normalized 0..1 -> param.min..max)
        elif name == "set_clip_automation_points":
            res = send_mcp_command("set_clip_automation_points", {
                "track_index": _zb(args.get("track_index", 1)),
                "clip_index": _zb(args.get("clip_index", 1)),
                "parameter_name": str(args.get("parameter_name", "volume")),
                "points": args.get("points", []),
                "is_arrangement": bool(args.get("is_arrangement", False)),
                "clear_existing": bool(args.get("clear_existing", True))
            })
        elif name == "manage_clip_automation":
            res = send_mcp_command("manage_clip_automation", {
                "track_index": _zb(args.get("track_index", 1)),
                "clip_index": _zb(args.get("clip_index", 1)),
                "action": str(args.get("action", "create")),
                "parameter_name": str(args.get("parameter_name", "volume"))
            })
        # Hardware MIDI Bridge (CC / Program Change / Note to external gear)
        elif name == "send_hardware_midi":
            msg_type = str(args.get("msg_type", "cc")).lower()
            ch = max(1, min(16, int(args.get("channel", 1)))) - 1
            if msg_type == "pc":
                status = 192 + ch
                d1 = int(args.get("program", 0))
                d2 = 0
            elif msg_type == "note_on":
                status = 144 + ch
                d1 = int(args.get("note", 60))
                d2 = int(args.get("velocity", 100))
            else:  # cc
                status = 176 + ch
                d1 = int(args.get("cc", 74))
                d2 = int(args.get("value", 64))
            res = send_mcp_command("send_hardware_midi", {"status_byte": status, "data1": d1, "data2": d2})
        # Browser & Devices & Mixer
        elif name == "get_browser_items_at_path":
            res = send_mcp_command("get_browser_items_at_path", {"path": str(args.get("path", "instruments"))})
        elif name == "load_browser_item":
            res = send_mcp_command("load_browser_item", {"track_index": _zb(args.get("track_index", 1)), "item_uri": str(args.get("item_uri", ""))})
        elif name == "get_device_parameters":
            res = send_mcp_command("get_device_parameters", {"track_index": _zb(args.get("track_index", 1)), "device_index": _zb(args.get("device_index", 1))})
        elif name == "set_device_parameter":
            res = send_mcp_command("set_device_parameter", {
                "track_index": _zb(args.get("track_index", 1)),
                "device_index": _zb(args.get("device_index", 1)),
                "parameter_name": str(args.get("parameter_name", "")),
                "value": float(args.get("value", 0.5))
            })
        elif name == "set_track_volume":
            res = send_mcp_command("set_track_volume", {"track_index": _zb(args.get("track_index", 1)), "volume": float(args.get("volume", 0.85))})
        elif name == "set_track_panning":
            res = send_mcp_command("set_track_panning", {"track_index": _zb(args.get("track_index", 1)), "panning": float(args.get("panning", 0.0))})
        else:
            res = {"status": "error", "message": f"Unknown tool: {name}"}
        return json.dumps(res)[:3500]
    except Exception as e:
        return json.dumps({"status": "error", "message": str(e)})


ABLETON_TOOLS = [
    {"type": "function", "function": {"name": "get_session_info", "description": "Get Ableton session tracks, tempo and clips.", "parameters": {"type": "object", "properties": {}}}},
    {"type": "function", "function": {"name": "get_track_info", "description": "Get details of a track (1-based).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}}, "required": ["track_index"]}}},
    {"type": "function", "function": {"name": "set_tempo", "description": "Set Ableton BPM tempo.", "parameters": {"type": "object", "properties": {"tempo": {"type": "number"}}, "required": ["tempo"]}}},
    {"type": "function", "function": {"name": "create_midi_track", "description": "Create a MIDI track (-1 appends at end).", "parameters": {"type": "object", "properties": {"index": {"type": "integer"}}}}},
    {"type": "function", "function": {"name": "set_track_name", "description": "Rename a track (1-based).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "name": {"type": "string"}}, "required": ["track_index", "name"]}}},
    {"type": "function", "function": {"name": "set_track_arm", "description": "Arm/disarm a track for MIDI or hardware recording (1-based).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "arm": {"type": "boolean"}}, "required": ["track_index", "arm"]}}},
    {"type": "function", "function": {"name": "create_clip", "description": "Create a Session MIDI clip (1-based track_index, clip_index, length in beats).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}, "length": {"type": "number"}}, "required": ["track_index", "clip_index", "length"]}}},
    {"type": "function", "function": {"name": "generate_musical_pattern", "description": "Smart music theory generator: writes musical rolling_bass, arp or chords with humanized velocity (1-127) in any key.", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}, "key": {"type": "string"}, "style": {"type": "string", "enum": ["rolling_bass", "arp", "chords"]}, "bars": {"type": "integer"}, "intensity": {"type": "number"}}, "required": ["track_index", "clip_index", "key", "style"]}}},
    {"type": "function", "function": {"name": "add_notes_to_clip", "description": "Add custom MIDI notes with pitch, start_time, duration and velocity (1-127) to a Session clip.", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}, "notes": {"type": "array", "items": {"type": "object", "properties": {"pitch": {"type": "integer"}, "start_time": {"type": "number"}, "duration": {"type": "number"}, "velocity": {"type": "integer"}}}}}, "required": ["track_index", "clip_index", "notes"]}}},
    {"type": "function", "function": {"name": "fire_clip", "description": "Launch a Session clip.", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}}, "required": ["track_index", "clip_index"]}}},
    {"type": "function", "function": {"name": "get_arrangement_info", "description": "Inspect Arrangement timeline clips, loop and cue points.", "parameters": {"type": "object", "properties": {}}}},
    {"type": "function", "function": {"name": "set_view", "description": "Switch Ableton view between 'Session' and 'Arranger'.", "parameters": {"type": "object", "properties": {"view_name": {"type": "string", "enum": ["Session", "Arranger"]}}, "required": ["view_name"]}}},
    {"type": "function", "function": {"name": "set_song_time", "description": "Move Arrangement playhead cursor to exact beat position.", "parameters": {"type": "object", "properties": {"time": {"type": "number"}}, "required": ["time"]}}},
    {"type": "function", "function": {"name": "create_arrangement_clip", "description": "Create a MIDI clip directly on Arrangement timeline at start_time (beats).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "start_time": {"type": "number"}, "length": {"type": "number"}}, "required": ["track_index", "start_time", "length"]}}},
    {"type": "function", "function": {"name": "duplicate_to_arrangement", "description": "Place a Session clip onto the Arrangement timeline at start_time (beats).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}, "start_time": {"type": "number"}}, "required": ["track_index", "clip_index", "start_time"]}}},
    {"type": "function", "function": {"name": "delete_arrangement_clip", "description": "Delete a clip from Arrangement timeline.", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}}, "required": ["track_index", "clip_index"]}}},
    {"type": "function", "function": {"name": "set_arrangement_clip_property", "description": "Set Arrangement clip property (looping, loop_start, loop_end, muted, name).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}, "property": {"type": "string"}, "value": {}}, "required": ["track_index", "clip_index", "property", "value"]}}},
    {"type": "function", "function": {"name": "set_arrangement_loop", "description": "Configure Arrangement loop brace.", "parameters": {"type": "object", "properties": {"start": {"type": "number"}, "length": {"type": "number"}, "enabled": {"type": "boolean"}}, "required": ["start", "length", "enabled"]}}},
    {"type": "function", "function": {"name": "create_cue_point", "description": "Add Arrangement locator marker (Intro, Build, Drop, Break).", "parameters": {"type": "object", "properties": {"time": {"type": "number"}, "name": {"type": "string"}}, "required": ["time", "name"]}}},
    {"type": "function", "function": {"name": "jump_to_cue", "description": "Jump playback cursor to a cue point (1-based).", "parameters": {"type": "object", "properties": {"index": {"type": "integer"}}, "required": ["index"]}}},
    {"type": "function", "function": {"name": "set_clip_automation_points", "description": "Place accurate automation envelope points (time in beats, value 0.0-1.0) on Session or Arrangement clips for any mixer/device parameter (e.g. 'Volume', 'Frequency', 'Cutoff').", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}, "parameter_name": {"type": "string"}, "is_arrangement": {"type": "boolean"}, "points": {"type": "array", "items": {"type": "object", "properties": {"time": {"type": "number"}, "value": {"type": "number"}}, "required": ["time", "value"]}}}, "required": ["track_index", "clip_index", "parameter_name", "points"]}}},
    {"type": "function", "function": {"name": "send_hardware_midi", "description": "Send MIDI CC, Program Change (pc) or Note directly to connected hardware synths/controllers via Ableton MIDI Out.", "parameters": {"type": "object", "properties": {"msg_type": {"type": "string", "enum": ["cc", "pc", "note_on"]}, "channel": {"type": "integer"}, "cc": {"type": "integer"}, "value": {"type": "integer"}, "program": {"type": "integer"}, "note": {"type": "integer"}, "velocity": {"type": "integer"}}, "required": ["msg_type"]}}},
    {"type": "function", "function": {"name": "get_browser_items_at_path", "description": "Browse Ableton instruments/effects.", "parameters": {"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}}},
    {"type": "function", "function": {"name": "load_browser_item", "description": "Load instrument or effect URI onto a track (1-based).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "item_uri": {"type": "string"}}, "required": ["track_index", "item_uri"]}}},
    {"type": "function", "function": {"name": "get_device_parameters", "description": "List device parameters on a track (1-based).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "device_index": {"type": "integer"}}, "required": ["track_index", "device_index"]}}},
    {"type": "function", "function": {"name": "set_device_parameter", "description": "Set normalized parameter (0.0-1.0) on a device.", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "device_index": {"type": "integer"}, "parameter_name": {"type": "string"}, "value": {"type": "number"}}, "required": ["track_index", "device_index", "parameter_name", "value"]}}},
    {"type": "function", "function": {"name": "set_track_volume", "description": "Set track volume (0.0-1.0).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "volume": {"type": "number"}}, "required": ["track_index", "volume"]}}},
    {"type": "function", "function": {"name": "set_track_panning", "description": "Set track panning (-1.0 to +1.0).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "panning": {"type": "number"}}, "required": ["track_index", "panning"]}}}
]


def run_ableton_mcp_agent(user_prompt: str, session_context: str = "", status_callback=None) -> str:
    sys_prompt = (
        "Sei NEURAL-LAB AI Producer & Studio Engineer per Ableton Live (Electronic / Melodic Techno / Club Music). "
        "Hai controllo completo su: "
        "1) Composizione musicale intelligente (generate_musical_pattern per rolling_bass, arp, chords in tonalita' con velocity dinamiche 1-127, oppure add_notes_to_clip). "
        "2) Automation Envelope precisi (set_clip_automation_points con punti time in beat e value normalizzato 0.0-1.0 per Volume, Frequency, Cutoff, Send). "
        "3) Full Arrangement Timeline (set_view 'Arranger', create_arrangement_clip, duplicate_to_arrangement, set_arrangement_loop, create_cue_point, set_song_time). "
        "4) Hardware MIDI Bridge (send_hardware_midi per inviare CC, Program Change o Note a synth esterni e set_track_arm per armare tracce hardware). "
        "Indici di tracce/clip sono 1-based. Rispondi in italiano in modo conciso e musicale (max 2 frasi). "
        f"Contesto sessione: {session_context}"
    )

    messages = [{"role": "system", "content": sys_prompt}, {"role": "user", "content": user_prompt}]

    for _ in range(7):
        payload = {
            "model": AGENT_MODEL,
            "messages": messages,
            "tools": ABLETON_TOOLS,
            "stream": False,
            "options": {"num_ctx": 4096, "temperature": 0.25}
        }
        req = urllib.request.Request(OLLAMA_CHAT_URL, data=json.dumps(payload).encode("utf-8"), headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=25.0) as resp:
            data = json.loads(resp.read().decode("utf-8"))

        msg = data.get("message", {})
        messages.append(msg)
        tool_calls = msg.get("tool_calls") or []
        if not tool_calls:
            final_text = (msg.get("content") or "").strip()
            return final_text if final_text else "Operazione completata su Ableton Live."

        for tc in tool_calls:
            fn = tc.get("function", {})
            fn_name = fn.get("name", "")
            fn_args = fn.get("arguments", {})
            if isinstance(fn_args, str):
                try:
                    fn_args = json.loads(fn_args)
                except Exception:
                    fn_args = {}
            if status_callback:
                status_callback(f"MCP: {fn_name.upper()}...")
            messages.append({"role": "tool", "content": execute_tool(fn_name, fn_args)})

    return "Sequenza Ableton MCP completata."