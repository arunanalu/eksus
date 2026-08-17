extends Node

## Esta e a unica API Exus que as mecanicas do TPS podem chamar.
const EventContract = preload("res://exus/exus_event.gd")
const UdpClientScript = preload("res://exus/exus_udp_client.gd")
signal event_emitted(payload: Dictionary)
signal bridge_result_received(result: Dictionary)
signal delivery_state_changed(state: String, detail: String)

var session_id := ""
var next_seq := 0
var udp_client
var active_streams: Dictionary = {}


func _ready() -> void:
	session_id = "godot-%d-%08x" % [Time.get_unix_time_from_system(), randi()]
	udp_client = UdpClientScript.new()
	add_child(udp_client)
	udp_client.bridge_result_received.connect(_on_bridge_result)
	udp_client.request_timed_out.connect(_on_request_timed_out)
	udp_client.socket_error.connect(func(message: String): delivery_state_changed.emit("CONTROL AUSENTE", message))
	delivery_state_changed.emit("CONTROL AUSENTE", "Aguardando o Exus Control em 127.0.0.1:4242")


func oneshot(event_name: String, magnitude: float, duration_ms: int, azimuth_deg: Variant = null,
		source := "gameplay") -> Dictionary:
	return _emit(event_name, "oneshot", magnitude, duration_ms, azimuth_deg, source)


func start_stream(event_name: String, stream_id: String, magnitude: float, azimuth_deg: Variant = null,
		source := "gameplay") -> Dictionary:
	var payload := _emit(event_name, "start", magnitude, null, azimuth_deg, source, stream_id)
	active_streams[stream_id] = {"event": event_name, "source": source}
	return payload


func update_stream(event_name: String, stream_id: String, magnitude: float, azimuth_deg: Variant = null,
		source := "gameplay") -> Dictionary:
	if not active_streams.has(stream_id):
		return start_stream(event_name, stream_id, magnitude, azimuth_deg, source)
	return _emit(event_name, "update", magnitude, null, azimuth_deg, source, stream_id)


func stop_stream(event_name: String, stream_id: String, source := "gameplay") -> Dictionary:
	active_streams.erase(stream_id)
	return _emit(event_name, "stop", 0.0, null, null, source, stream_id)


func stop_all_streams() -> void:
	for stream_id in active_streams.keys().duplicate():
		var stream: Dictionary = active_streams[stream_id]
		stop_stream(stream.event, stream_id, stream.source)


func azimuth_from_positions(player_position: Vector3, player_basis: Basis, source_position: Vector3) -> Variant:
	var direction := source_position - player_position
	direction.y = 0.0
	if direction.length_squared() < 0.0001:
		return null
	var local_direction := player_basis.inverse() * direction.normalized()
	return clampf(rad_to_deg(atan2(local_direction.x, -local_direction.z)), -180.0, 180.0)


func _emit(event_name: String, state: String, magnitude: float, duration_ms: Variant, azimuth_deg: Variant,
		source: String, stream_id: Variant = null) -> Dictionary:
	var payload: Dictionary = EventContract.build(session_id, next_seq, event_name, state, clampf(magnitude, 0.0, 1.0),
		duration_ms, azimuth_deg, source, bool(ExusSettings.get_value("real_vibration_requested")), stream_id)
	next_seq += 1
	print("[EXUS EVENT] ", JSON.stringify(payload))
	event_emitted.emit(payload)
	if not ExusSettings.get_value("integration_enabled"):
		delivery_state_changed.emit("SIMULADO", "Integracao UDP desativada no jogo")
		return payload
	udp_client.send_event(payload, str(ExusSettings.get_value("host")), int(ExusSettings.get_value("port")))
	return payload


func _on_bridge_result(result: Dictionary) -> void:
	var result_name := str(result.get("result", "rejected"))
	var state := "REJEITADO"
	if result_name == "simulated":
		state = "SIMULADO"
	elif result_name == "sent":
		state = "ENVIADO"
	delivery_state_changed.emit(state, str(result.get("reason", result_name)))
	bridge_result_received.emit(result)


func _on_request_timed_out(seq: int) -> void:
	delivery_state_changed.emit("CONTROL AUSENTE", "Sem resposta para seq %d" % seq)


func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_CLOSE_REQUEST:
		stop_all_streams()
