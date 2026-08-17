extends Node2D

const WORLD := Rect2(-1650, -1150, 3300, 2300)
const ICEBERGS := [
	{"pos": Vector2(-860, -420), "radius": 48.0}, {"pos": Vector2(-430, 510), "radius": 36.0},
	{"pos": Vector2(220, -650), "radius": 42.0}, {"pos": Vector2(720, 430), "radius": 54.0},
	{"pos": Vector2(1230, -300), "radius": 38.0}, {"pos": Vector2(1050, 800), "radius": 45.0},
	{"pos": Vector2(-1200, 720), "radius": 34.0}, {"pos": Vector2(360, 880), "radius": 31.0}
]
const ISLANDS := [
	{"pos": Vector2(760, -520), "radius": 190.0},
	{"pos": Vector2(-1120, 180), "radius": 145.0},
	{"pos": Vector2(120, 980), "radius": 125.0}
]
const ISLAND_TEXTURE := preload("res://assets/island_chromakey.png")
const ICEBERG_TEXTURE := preload("res://assets/iceberg_chromakey.png")
const CHROMA_SHADER := preload("res://shaders/chroma_key.gdshader")

@onready var boat: Node2D = $Boat
@onready var boat_sail: Sprite2D = $Boat/Sail
@onready var wind_label: Label = $HUD/Wind
@onready var status_label: Label = $HUD/Status
@onready var flash: ColorRect = $HUD/Flash
@onready var sea_material: ShaderMaterial = $Sea.material
@onready var sail_material: ShaderMaterial = boat_sail.material

var heading := 0.0
var velocity := Vector2.ZERO
var sail_trim := 0.85
var displayed_sail_trim := 0.85
var wind_world := deg_to_rad(-62.0)
var elapsed := 0.0
var wind_timer := 0.0
var flash_amount := 0.0
var debug_visible := true
var last_hit: Dictionary = {}
var last_island_hit: Dictionary = {}

func _ready() -> void:
	_apply_visual_style()
	boat.position = Vector2.ZERO
	_create_world_sprites()
	ExusBridge.start_wind(_relative_wind_deg(), _wind_magnitude())

func _apply_visual_style() -> void:
	var file := FileAccess.open("res://config/visual/sea_style.json", FileAccess.READ)
	if file == null:
		return
	var parsed = JSON.parse_string(file.get_as_text())
	if typeof(parsed) != TYPE_DICTIONARY:
		return
	var palette: Dictionary = parsed.get("palette", {})
	var waves: Dictionary = parsed.get("waves", {})
	sea_material.set_shader_parameter("deep_color", Color.from_string(str(palette.get("deep", "#041f3f")), Color("#041f3f")))
	sea_material.set_shader_parameter("horizon_color", Color.from_string(str(palette.get("horizon", "#086197")), Color("#086197")))
	sea_material.set_shader_parameter("sun_color", Color.from_string(str(palette.get("sun", "#ffc86b")), Color("#ffc86b")))
	sea_material.set_shader_parameter("wave_scale", float(waves.get("wave_scale", 3.8)))
	sea_material.set_shader_parameter("far_speed", float(waves.get("far_speed", 0.12)))
	sea_material.set_shader_parameter("near_speed", float(waves.get("near_speed", 0.24)))

func _exit_tree() -> void:
	ExusBridge.stop_wind()

func _process(delta: float) -> void:
	elapsed += delta
	# Brisa predominante: mudanças legíveis em dezenas de segundos, não a cada curva.
	# A segunda onda introduz viradas maiores, porém raras e sempre interpoladas.
	var wind_target: float = deg_to_rad(-28.0 + sin(elapsed * 0.105) * 62.0 + sin(elapsed * 0.027) * 28.0)
	wind_world = lerp_angle(wind_world, wind_target, minf(1.0, delta * 0.18))
	var steer := _axis(KEY_A, KEY_D) + _axis(KEY_LEFT, KEY_RIGHT)
	heading += steer * delta * (1.9 + velocity.length() * 0.005)
	sail_trim = clampf(sail_trim + _axis(KEY_S, KEY_W) * delta * 0.52, 0.0, 1.0)
	var drive: float = abs(sin(wind_world - heading)) * sail_trim
	var desired: Vector2 = Vector2.UP.rotated(heading) * (205.0 * drive)
	velocity = velocity.lerp(desired, minf(1.0, delta * (0.55 if sail_trim > 0.02 else 1.35)))
	boat.position += velocity * delta
	boat.position = Vector2(clampf(boat.position.x, WORLD.position.x, WORLD.end.x), clampf(boat.position.y, WORLD.position.y, WORLD.end.y))
	boat.rotation = heading
	_check_icebergs()
	_check_islands()
	sea_material.set_shader_parameter("wind_dir", Vector2(cos(wind_world), sin(wind_world)))
	displayed_sail_trim = move_toward(displayed_sail_trim, sail_trim, delta * 1.8)
	sail_material.set_shader_parameter("sail_amount", displayed_sail_trim)
	wind_timer += delta
	if wind_timer >= ExusBridge.wind_update_interval():
		wind_timer = 0.0
		ExusBridge.update_wind(_relative_wind_deg(), _wind_magnitude())
	if Input.is_key_pressed(KEY_F8):
		debug_visible = true
	if Input.is_key_pressed(KEY_F7):
		ExusBridge.real_output_requested = false
	flash_amount = maxf(0.0, flash_amount - delta * 2.3)
	flash.color.a = flash_amount
	_update_hud()

func _axis(negative_key: Key, positive_key: Key) -> float:
	return float(Input.is_key_pressed(positive_key)) - float(Input.is_key_pressed(negative_key))

func _relative_wind_deg() -> float:
	return wrapf(rad_to_deg(wind_world - heading), -180.0, 180.0)

func _wind_magnitude() -> float:
	return clampf(0.48 + 0.26 * sin(elapsed * 0.23) + 0.16 * sin(elapsed * 0.51), 0.18, 0.92)

func _check_icebergs() -> void:
	for index in ICEBERGS.size():
		var iceberg: Dictionary = ICEBERGS[index]
		if boat.position.distance_to(iceberg.pos) >= iceberg.radius + 42.0:
			continue
		var now := Time.get_ticks_msec()
		if now - int(last_hit.get(index, 0)) < 800:
			continue
		last_hit[index] = now
		var direction: Vector2 = (iceberg.pos - boat.position).normalized()
		var forward := Vector2.UP.rotated(heading)
		var azimuth := wrapf(rad_to_deg(direction.angle() - forward.angle()), -180.0, 180.0)
		ExusBridge.ice_collision(azimuth, clampf(velocity.length() / 180.0, 0.38, 1.0))
		velocity *= -0.23
		flash_amount = 0.28

func _check_islands() -> void:
	for index in ISLANDS.size():
		var island: Dictionary = ISLANDS[index]
		var offset: Vector2 = boat.position - island.pos
		var safe_distance: float = island.radius + 47.0
		if offset.length() >= safe_distance:
			continue
		var normal := offset.normalized()
		if normal.length_squared() == 0.0:
			normal = Vector2.UP
		boat.position = island.pos + normal * safe_distance
		velocity = velocity.slide(normal) * 0.18
		var now := Time.get_ticks_msec()
		if now - int(last_island_hit.get(index, 0)) < 800:
			continue
		last_island_hit[index] = now
		var forward := Vector2.UP.rotated(heading)
		var azimuth := wrapf(rad_to_deg((-normal).angle() - forward.angle()), -180.0, 180.0)
		ExusBridge.ice_collision(azimuth, clampf(velocity.length() / 150.0, 0.30, 0.75))
		flash_amount = 0.18

func _create_world_sprites() -> void:
	for island in ISLANDS:
		var sprite := Sprite2D.new()
		sprite.texture = ISLAND_TEXTURE
		sprite.material = _chroma_material()
		sprite.position = island.pos
		sprite.scale = Vector2.ONE * (island.radius * 2.0 / 1040.0)
		add_child(sprite)
	for iceberg in ICEBERGS:
		var sprite := Sprite2D.new()
		sprite.texture = ICEBERG_TEXTURE
		sprite.material = _chroma_material()
		sprite.position = iceberg.pos
		sprite.scale = Vector2.ONE * (iceberg.radius * 2.0 / 930.0)
		add_child(sprite)

func _chroma_material() -> ShaderMaterial:
	var material := ShaderMaterial.new()
	material.shader = CHROMA_SHADER
	return material

func _update_hud() -> void:
	var degrees := _relative_wind_deg()
	var direction := "TESTA"
	if degrees < -28.0:
		direction = "ESQUERDA"
	elif degrees > 28.0:
		direction = "DIREITA"
	wind_label.text = "VENTO  %s  •  %d°  •  %d%%" % [direction, roundi(degrees), roundi(_wind_magnitude() * 100.0)]
	status_label.visible = debug_visible
	status_label.text = "Vela %d%%  |  Exus: %s  |  Perfil: boat-demo/v1" % [roundi(sail_trim * 100.0), ExusBridge.last_result]
