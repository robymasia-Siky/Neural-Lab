import json
import socket
import urllib.request

ABLETON_MCP_HOST = "127.0.0.1"
ABLETON_MCP_PORT = 9877
OLLAMA_CHAT_URL = "http://127.0.0.1:11434/api/chat"
AGENT_MODEL = "qwen2.5-coder:3b"


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
            res = send_mcp_command("set_track_name", {
                "track_index": _zb(args.get("track_index", 1)),
                "name": str(args.get("name", "AI Track"))
            })
        elif name == "delete_track":
            res = send_mcp_command("delete_track", {"track_index": _zb(args.get("track_index", 1))})
        elif name == "create_clip":
            res = send_mcp_command("create_clip", {
                "track_index": _zb(args.get("track_index", 1)),
                "clip_index": _zb(args.get("clip_index", 1)),
                "length": float(args.get("length", 16.0))
            })
        elif name == "add_notes_to_clip":
            notes = args.get("notes", [])
            norm_notes = []
            for n in notes:
                norm_notes.append({
                    "pitch": int(n.get("pitch", 60)),
                    "start_time": float(n.get("start_time", 0.0)),
                    "duration": float(n.get("duration", 0.25)),
                    "velocity": max(1, min(127, int(n.get("velocity", 100)))),
                    "mute": bool(n.get("mute", False))
                })
            res = send_mcp_command("add_notes_to_clip", {
                "track_index": _zb(args.get("track_index", 1)),
                "clip_index": _zb(args.get("clip_index", 1)),
                "notes": norm_notes
            })
        elif name == "set_clip_name":
            res = send_mcp_command("set_clip_name", {
                "track_index": _zb(args.get("track_index", 1)),
                "clip_index": _zb(args.get("clip_index", 1)),
                "name": str(args.get("name", "AI Clip"))
            })
        elif name == "fire_clip":
            res = send_mcp_command("fire_clip", {
                "track_index": _zb(args.get("track_index", 1)),
                "clip_index": _zb(args.get("clip_index", 1))
            })
        elif name == "stop_clip":
            res = send_mcp_command("stop_clip", {
                "track_index": _zb(args.get("track_index", 1)),
                "clip_index": _zb(args.get("clip_index", 1))
            })
        elif name == "start_playback":
            res = send_mcp_command("start_playback")
        elif name == "stop_playback":
            res = send_mcp_command("stop_playback")
        elif name == "get_arrangement_info":
            res = send_mcp_command("get_arrangement_info")
        elif name == "create_arrangement_clip":
            res = send_mcp_command("create_arrangement_clip", {
                "track_index": _zb(args.get("track_index", 1)),
                "start_time": float(args.get("start_time", 0.0)),
                "length": float(args.get("length", 16.0))
            })
        elif name == "add_notes_to_arrangement_clip":
            res = send_mcp_command("add_notes_to_arrangement_clip", {
                "track_index": _zb(args.get("track_index", 1)),
                "clip_index": _zb(args.get("clip_index", 1)),
                "notes": args.get("notes", [])
            })
        elif name == "duplicate_to_arrangement":
            res = send_mcp_command("duplicate_to_arrangement", {
                "track_index": _zb(args.get("track_index", 1)),
                "clip_index": _zb(args.get("clip_index", 1)),
                "start_time": float(args.get("start_time", 0.0))
            })
        elif name == "set_arrangement_loop":
            res = send_mcp_command("set_arrangement_loop", {
                "start": float(args.get("start", 0.0)),
                "length": float(args.get("length", 64.0)),
                "enabled": bool(args.get("enabled", True))
            })
        elif name == "create_cue_point":
            res = send_mcp_command("create_cue_point", {
                "time": float(args.get("time", 0.0)),
                "name": str(args.get("name", "Section"))
            })
        elif name == "manage_clip_automation":
            res = send_mcp_command("manage_clip_automation", {
                "track_index": _zb(args.get("track_index", 1)),
                "clip_index": _zb(args.get("clip_index", 1)),
                "clip_name": str(args.get("clip_name", "")),
                "action": str(args.get("action", "create")),
                "parameter_name": str(args.get("parameter_name", "volume"))
            })
        elif name == "get_browser_items_at_path":
            res = send_mcp_command("get_browser_items_at_path", {"path": str(args.get("path", "instruments"))})
        elif name == "load_browser_item":
            res = send_mcp_command("load_browser_item", {
                "track_index": _zb(args.get("track_index", 1)),
                "item_uri": str(args.get("item_uri", ""))
            })
        elif name == "get_device_parameters":
            res = send_mcp_command("get_device_parameters", {
                "track_index": _zb(args.get("track_index", 1)),
                "device_index": _zb(args.get("device_index", 1))
            })
        elif name == "set_device_parameter":
            res = send_mcp_command("set_device_parameter", {
                "track_index": _zb(args.get("track_index", 1)),
                "device_index": _zb(args.get("device_index", 1)),
                "chain_index": int(args.get("chain_index", 0)),
                "parameter_name": str(args.get("parameter_name", "")),
                "parameter_index": _zb(args.get("parameter_index", 1)) if args.get("parameter_index") else 0,
                "value": float(args.get("value", 0.5))
            })
        elif name == "set_track_volume":
            res = send_mcp_command("set_track_volume", {
                "track_index": _zb(args.get("track_index", 1)),
                "volume": float(args.get("volume", 0.85))
            })
        elif name == "set_track_panning":
            res = send_mcp_command("set_track_panning", {
                "track_index": _zb(args.get("track_index", 1)),
                "panning": float(args.get("panning", 0.0))
            })
        else:
            res = {"status": "error", "message": f"Unknown tool: {name}"}
        return json.dumps(res)[:3500]
    except Exception as e:
        return json.dumps({"status": "error", "message": str(e)})


ABLETON_TOOLS = [
    {"type": "function", "function": {"name": "get_session_info", "description": "Get current Ableton Live session info (tracks, tempo, clips).", "parameters": {"type": "object", "properties": {}}}},
    {"type": "function", "function": {"name": "get_track_info", "description": "Get details of a specific track (1-based index).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}}, "required": ["track_index"]}}},
    {"type": "function", "function": {"name": "set_tempo", "description": "Set Ableton Live BPM tempo.", "parameters": {"type": "object", "properties": {"tempo": {"type": "number"}}, "required": ["tempo"]}}},
    {"type": "function", "function": {"name": "create_midi_track", "description": "Create a new MIDI track (-1 appends at end).", "parameters": {"type": "object", "properties": {"index": {"type": "integer"}}}}},
    {"type": "function", "function": {"name": "set_track_name", "description": "Rename a track (1-based index).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "name": {"type": "string"}}, "required": ["track_index", "name"]}}},
    {"type": "function", "function": {"name": "delete_track", "description": "Delete a track (1-based index).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}}, "required": ["track_index"]}}},
    {"type": "function", "function": {"name": "create_clip", "description": "Create an empty MIDI clip in Session view (1-based track_index, clip_index, length in beats).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}, "length": {"type": "number"}}, "required": ["track_index", "clip_index", "length"]}}},
    {"type": "function", "function": {"name": "add_notes_to_clip", "description": "Add MIDI notes with velocity dynamics (1-127) to a Session clip.", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}, "notes": {"type": "array", "items": {"type": "object", "properties": {"pitch": {"type": "integer"}, "start_time": {"type": "number"}, "duration": {"type": "number"}, "velocity": {"type": "integer"}}, "required": ["pitch", "start_time", "duration", "velocity"]}}}, "required": ["track_index", "clip_index", "notes"]}}},
    {"type": "function", "function": {"name": "set_clip_name", "description": "Rename a Session clip.", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}, "name": {"type": "string"}}, "required": ["track_index", "clip_index", "name"]}}},
    {"type": "function", "function": {"name": "fire_clip", "description": "Launch a Session clip.", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}}, "required": ["track_index", "clip_index"]}}},
    {"type": "function", "function": {"name": "stop_clip", "description": "Stop a Session clip.", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}}, "required": ["track_index", "clip_index"]}}},
    {"type": "function", "function": {"name": "start_playback", "description": "Start Ableton playback.", "parameters": {"type": "object", "properties": {}}}},
    {"type": "function", "function": {"name": "stop_playback", "description": "Stop Ableton playback.", "parameters": {"type": "object", "properties": {}}}},
    {"type": "function", "function": {"name": "get_arrangement_info", "description": "Get Arrangement view clips, loop and cue points.", "parameters": {"type": "object", "properties": {}}}},
    {"type": "function", "function": {"name": "create_arrangement_clip", "description": "Create a MIDI clip on Arrangement timeline (start_time and length in beats).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "start_time": {"type": "number"}, "length": {"type": "number"}}, "required": ["track_index", "start_time", "length"]}}},
    {"type": "function", "function": {"name": "add_notes_to_arrangement_clip", "description": "Add MIDI notes with velocity to an Arrangement clip.", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}, "notes": {"type": "array", "items": {"type": "object"}}}, "required": ["track_index", "clip_index", "notes"]}}},
    {"type": "function", "function": {"name": "duplicate_to_arrangement", "description": "Duplicate a Session clip to Arrangement timeline at start_time (beats).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}, "start_time": {"type": "number"}}, "required": ["track_index", "clip_index", "start_time"]}}},
    {"type": "function", "function": {"name": "set_arrangement_loop", "description": "Set Arrangement loop brace.", "parameters": {"type": "object", "properties": {"start": {"type": "number"}, "length": {"type": "number"}, "enabled": {"type": "boolean"}}, "required": ["start", "length", "enabled"]}}},
    {"type": "function", "function": {"name": "create_cue_point", "description": "Create Arrangement locator marker (Intro, Drop, Break).", "parameters": {"type": "object", "properties": {"time": {"type": "number"}, "name": {"type": "string"}}, "required": ["time", "name"]}}},
    {"type": "function", "function": {"name": "manage_clip_automation", "description": "Create or clear automation envelopes on Arrangement clips (action: create, clear, clear_all).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "clip_index": {"type": "integer"}, "clip_name": {"type": "string"}, "action": {"type": "string"}, "parameter_name": {"type": "string"}}, "required": ["track_index", "action", "parameter_name"]}}},
    {"type": "function", "function": {"name": "get_browser_items_at_path", "description": "Browse Ableton instruments/effects (e.g. 'instruments/Wavetable', 'audio_effects').", "parameters": {"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}}},
    {"type": "function", "function": {"name": "load_browser_item", "description": "Load an instrument or effect URI onto a track (1-based).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "item_uri": {"type": "string"}}, "required": ["track_index", "item_uri"]}}},
    {"type": "function", "function": {"name": "get_device_parameters", "description": "List parameters of a device on a track (1-based).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "device_index": {"type": "integer"}}, "required": ["track_index", "device_index"]}}},
    {"type": "function", "function": {"name": "set_device_parameter", "description": "Set normalized parameter (0.0-1.0) on a device.", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "device_index": {"type": "integer"}, "parameter_name": {"type": "string"}, "value": {"type": "number"}}, "required": ["track_index", "device_index", "parameter_name", "value"]}}},
    {"type": "function", "function": {"name": "set_track_volume", "description": "Set track volume (0.0 to 1.0, ~0.85 is 0dB).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "volume": {"type": "number"}}, "required": ["track_index", "volume"]}}},
    {"type": "function", "function": {"name": "set_track_panning", "description": "Set track panning (-1.0 left to +1.0 right).", "parameters": {"type": "object", "properties": {"track_index": {"type": "integer"}, "panning": {"type": "number"}}, "required": ["track_index", "panning"]}}}
]


def run_ableton_mcp_agent(user_prompt: str, session_context: str = "", status_callback=None) -> str:
    sys_prompt = (
        "Sei NEURAL-LAB AI Producer & Studio Engineer per Ableton Live. "
        "Usa i tool Ableton MCP per ispezionare la sessione, creare tracce MIDI, scrivere note con velocity dinamiche (1-127), "
        "costruire arrangiamenti, creare envelope di automazione (manage_clip_automation), caricare synth/effetti e regolare mixer/device. "
        "Gli indici di tracce e clip nei tool sono 1-based (la prima traccia e' 1). "
        "Rispondi in italiano in modo conciso, tecnico e musicale (massimo 2 frasi). "
        f"Contesto sessione: {session_context}"
    )

    messages = [
        {"role": "system", "content": sys_prompt},
        {"role": "user", "content": user_prompt}
    ]

    for step in range(6):
        payload = {
            "model": AGENT_MODEL,
            "messages": messages,
            "tools": ABLETON_TOOLS,
            "stream": False,
            "options": {"num_ctx": 4096, "temperature": 0.25}
        }
        req = urllib.request.Request(
            OLLAMA_CHAT_URL,
            data=json.dumps(payload).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
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
            result_str = execute_tool(fn_name, fn_args)
            messages.append({"role": "tool", "content": result_str})

    return "Sequenza Ableton MCP completata."