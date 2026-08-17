class_name ExusEvent
extends RefCounted

## Contrato exato aceito por Exus Control (exus.game-event/1).
const SCHEMA := "exus.game-event/1"
const VALID_EVENTS := ["damage", "explosion", "wind", "threat", "weapon_fire"]
const VALID_STATES := ["oneshot", "start", "update", "stop"]
const MAX_DURATION_MS := 2000


static func build(session_id: String, seq: int, event_name: String, state: String, magnitude: float,
		duration_ms: Variant, azimuth_deg: Variant, source: String, output_requested: bool,
		stream_id: Variant = null) -> Dictionary:
	var payload := {
		"schema": SCHEMA,
		"session_id": session_id,
		"seq": seq,
		"sent_at_ms": Time.get_ticks_msec(),
		"event": event_name,
		"state": state,
		"stream_id": stream_id,
		"azimuth_deg": azimuth_deg,
		"magnitude": magnitude,
		"duration_ms": duration_ms,
		"source": source,
		"output_requested": output_requested,
	}
	validate(payload)
	return payload


static func validate(payload: Dictionary) -> void:
	assert(payload.get("schema") == SCHEMA, "schema Exus invalido")
	assert(payload.get("session_id") is String and not payload.session_id.is_empty(), "session_id obrigatorio")
	assert(payload.get("seq") is int and payload.seq >= 0, "seq invalido")
	assert(payload.get("sent_at_ms") is int and payload.sent_at_ms >= 0, "sent_at_ms invalido")
	assert(payload.get("event") in VALID_EVENTS, "evento Exus invalido")
	assert(payload.get("state") in VALID_STATES, "estado Exus invalido")
	assert(payload.get("magnitude") is float or payload.get("magnitude") is int, "magnitude invalida")
	assert(is_finite(float(payload.magnitude)) and payload.magnitude >= 0.0 and payload.magnitude <= 1.0, "magnitude fora do intervalo")
	assert(payload.get("source") is String and not payload.source.is_empty(), "source obrigatorio")
	assert(payload.get("output_requested") is bool, "output_requested deve ser booleano")
	if payload.state == "oneshot":
		assert(payload.stream_id == null, "oneshot nao aceita stream_id")
		assert(payload.duration_ms is int and payload.duration_ms >= 1 and payload.duration_ms <= MAX_DURATION_MS, "duracao invalida")
	else:
		assert(payload.stream_id is String and not payload.stream_id.is_empty(), "stream_id obrigatorio")
		if payload.duration_ms != null:
			assert(payload.duration_ms is int and payload.duration_ms >= 1 and payload.duration_ms <= MAX_DURATION_MS, "duracao invalida")
	if payload.azimuth_deg != null:
		assert((payload.azimuth_deg is float or payload.azimuth_deg is int) and is_finite(float(payload.azimuth_deg)), "azimute invalido")
		assert(payload.azimuth_deg >= -180.0 and payload.azimuth_deg <= 180.0, "azimute fora do intervalo")
