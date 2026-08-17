extends SceneTree

const Event := preload("res://exus/exus_event.gd")


func _init() -> void:
	var shot := Event.build("test-session", 0, "weapon_fire", "oneshot", 0.4, 110, 0.0, "test", false)
	assert(shot.schema == "exus.game-event/1")
	assert(shot.output_requested == false)
	var wind := Event.build("test-session", 1, "wind", "start", 0.3, null, null, "test", false, "wind-1")
	assert(wind.stream_id == "wind-1")
	print("ExusEvent tests passed")
	quit(0)
