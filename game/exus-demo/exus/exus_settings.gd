extends Node

## Preferencias locais. Uma instalacao nova sempre comeca sem solicitar hardware.
const CONFIG_PATH := "user://exus_settings.cfg"
const DEFAULTS := {
	"integration_enabled": true,
	"real_vibration_requested": false,
	"host": "127.0.0.1",
	"port": 4242,
	"show_debug_overlay": true,
}

var config := ConfigFile.new()


func _ready() -> void:
	config.load(CONFIG_PATH)
	for key in DEFAULTS:
		if not config.has_section_key("exus", key):
			config.set_value("exus", key, DEFAULTS[key])
	save()


func get_value(key: String) -> Variant:
	return config.get_value("exus", key, DEFAULTS[key])


func set_value(key: String, value: Variant) -> void:
	if not DEFAULTS.has(key):
		push_error("Configuracao Exus desconhecida: " + key)
		return
	config.set_value("exus", key, value)
	save()


func save() -> void:
	config.save(CONFIG_PATH)
