extends SceneTree

## Executar com a ponte do Exus Control ativa em 127.0.0.1:4242.
const UdpClient := preload("res://exus/exus_udp_client.gd")

var completed := false


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	var client = UdpClient.new()
	root.add_child(client)
	client.bridge_result_received.connect(_on_result)
	client.socket_error.connect(func(message: String): push_error(message))
	client.send_event({
		"schema": "exus.game-event/1", "session_id": "godot-udp-test", "seq": 1,
		"sent_at_ms": 1, "event": "weapon_fire", "state": "oneshot", "stream_id": null,
		"azimuth_deg": 0.0, "magnitude": 0.4, "duration_ms": 110,
		"source": "godot_test", "output_requested": false,
	}, "127.0.0.1", 4242)
	await create_timer(2.0).timeout
	if not completed:
		push_error("a ponte Exus Control nao respondeu ao UDP")
		quit(1)
	else:
		quit(0)


func _on_result(result: Dictionary) -> void:
	if result.get("schema") != "exus.bridge-result/1" or result.get("result") != "simulated":
		push_error("resposta UDP inesperada: " + JSON.stringify(result))
		quit(1)
		return
	completed = true
	print("Exus UDP round-trip passed")
