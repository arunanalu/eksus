extends Node

## Instrumenta explosoes e ameacas traseiras sem alterar as regras de combate do TPS.
const THREAT_COOLDOWN_MS := 5000
var last_threat_ms := 0


func _process(_delta: float) -> void:
	var player := _local_player()
	if player == null or Time.get_ticks_msec() - last_threat_ms < THREAT_COOLDOWN_MS:
		return
	for robot in get_tree().get_nodes_in_group("exus_threat"):
		if not is_instance_valid(robot) or robot.dead:
			continue
		var offset: Vector3 = robot.global_position - player.global_position
		offset.y = 0.0
		if offset.length() > 13.0:
			continue
		var forward := -player.global_transform.basis.z
		if forward.dot(offset.normalized()) < -0.35:
			var azimuth: Variant = ExusEventBus.azimuth_from_positions(player.global_position, player.global_transform.basis, robot.global_position)
			ExusEventBus.oneshot("threat", 0.55, 260, azimuth, "red_robot_behind")
			last_threat_ms = Time.get_ticks_msec()
			return


func robot_exploded(robot: Node3D) -> void:
	var player := _local_player()
	var azimuth: Variant = null
	if player != null:
		azimuth = ExusEventBus.azimuth_from_positions(player.global_position, player.global_transform.basis, robot.global_position)
	ExusEventBus.oneshot("explosion", 0.95, 400, azimuth, "red_robot_destroyed")


func _local_player() -> Player:
	for node in get_tree().get_nodes_in_group("exus_player"):
		if node is Player and node.player_id == multiplayer.get_unique_id():
			return node
	return null
