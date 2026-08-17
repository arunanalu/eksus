extends Node

const HOST := "127.0.0.1"
const PORT := 4242
const PROFILE_ID := "boat-demo/v1"
const SCHEMA := "exus.game-event/1"

var _udp := PacketPeerUDP.new()
var _session_id := ""
var _sequence := 0
var _started_streams: Dictionary = {}
var last_result := "CONTROL AUSENTE"
var real_output_requested := false
var _profile_id := PROFILE_ID
var _wind_update_interval := 0.1

func _ready() -> void:
	_load_haptic_config()
	_session_id = "%x" % Time.get_ticks_msec()
	var error := _udp.bind(0, HOST)
	if error == OK:
		_udp.set_dest_address(HOST, PORT)
	else:
		last_result = "UDP INDISPONÍVEL"

func _process(_delta: float) -> void:
	while _udp.get_available_packet_count() > 0:
		var payload := _udp.get_packet().get_string_from_utf8()
		var parsed = JSON.parse_string(payload)
		if typeof(parsed) == TYPE_DICTIONARY:
			last_result = str(parsed.get("result", "SIMULADO")).to_upper()

func wind_update_interval() -> float:
	return _wind_update_interval

func _load_haptic_config() -> void:
	var file := FileAccess.open("res://config/haptics/boat-demo.v1.json", FileAccess.READ)
	if file == null:
		return
	var parsed = JSON.parse_string(file.get_as_text())
	if typeof(parsed) != TYPE_DICTIONARY:
		return
	_profile_id = str(parsed.get("profile_id", PROFILE_ID))
	var wind: Dictionary = parsed.get("wind", {})
	var update_hz := maxf(1.0, float(wind.get("update_hz", 10.0)))
	_wind_update_interval = 1.0 / update_hz

func start_wind(azimuth_deg: float, magnitude: float) -> void:
	_emit("wind", "start", azimuth_deg, magnitude, null, "boat-wind", "relative-wind")
	_started_streams["boat-wind"] = true

func update_wind(azimuth_deg: float, magnitude: float) -> void:
	if not _started_streams.has("boat-wind"):
		start_wind(azimuth_deg, magnitude)
		return
	_emit("wind", "update", azimuth_deg, magnitude, null, "boat-wind", "relative-wind")

func stop_wind() -> void:
	if _started_streams.erase("boat-wind"):
		_emit("wind", "stop", null, 0.0, null, "boat-wind", "scene-stop")

func ice_collision(azimuth_deg: float, magnitude: float) -> void:
	_emit("ice_collision", "oneshot", azimuth_deg, magnitude, 110, null, "iceberg")

func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_CLOSE_REQUEST:
		stop_wind()

func _emit(event_name: String, state: String, azimuth_deg, magnitude: float, duration_ms, stream_id, source: String) -> void:
	_sequence += 1
	var event := {
		"schema": SCHEMA,
		"session_id": _session_id,
		"seq": _sequence,
		"sent_at_ms": Time.get_ticks_msec(),
		"event": event_name,
		"state": state,
		"stream_id": stream_id,
		"azimuth_deg": azimuth_deg,
		"magnitude": clampf(magnitude, 0.0, 1.0),
		"duration_ms": duration_ms,
		"source": source,
		"haptic_profile": _profile_id,
		"output_requested": real_output_requested
	}
	_udp.put_packet(JSON.stringify(event).to_utf8_buffer())
