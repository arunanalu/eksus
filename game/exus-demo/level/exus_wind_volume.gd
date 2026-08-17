extends Area3D

## Volume real do nivel: start/update/stop a no maximo 10 Hz enquanto o jogador atravessa o vento.
@export var stream_id := "level-wind"
@export var magnitude := 0.35
@export var update_hz := 10.0

var player: Node3D
var elapsed := 0.0


func _ready() -> void:
	body_entered.connect(_on_body_entered)
	body_exited.connect(_on_body_exited)


func _physics_process(delta: float) -> void:
	if player == null:
		return
	elapsed += delta
	if elapsed >= 1.0 / update_hz:
		elapsed = 0.0
		ExusEventBus.update_stream("wind", stream_id, magnitude, 0.0, "wind_volume")


func _on_body_entered(body: Node3D) -> void:
	if body is Player and player == null:
		player = body
		elapsed = 0.0
		ExusEventBus.start_stream("wind", stream_id, magnitude, 0.0, "wind_volume")


func _on_body_exited(body: Node3D) -> void:
	if body == player:
		_stop()


func _exit_tree() -> void:
	_stop()


func _stop() -> void:
	if player != null:
		ExusEventBus.stop_stream("wind", stream_id, "wind_volume")
		player = null
