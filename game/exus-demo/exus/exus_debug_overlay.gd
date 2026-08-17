extends CanvasLayer

## Painel de desenvolvimento acessivel por F8. Tambem funciona sem Exus Control.
var panel: PanelContainer
var status_label: Label
var event_label: Label
var result_label: Label
var integration_toggle: CheckButton
var real_toggle: CheckButton


func _ready() -> void:
	layer = 20
	_build_ui()
	ExusEventBus.event_emitted.connect(_on_event_emitted)
	ExusEventBus.bridge_result_received.connect(_on_bridge_result)
	ExusEventBus.delivery_state_changed.connect(_on_delivery_state)


func _unhandled_key_input(input_event: InputEvent) -> void:
	if input_event is InputEventKey and input_event.pressed and not input_event.echo:
		if input_event.keycode == KEY_F8:
			panel.visible = not panel.visible
			get_viewport().set_input_as_handled()
		elif input_event.keycode == KEY_F7:
			panel.visible = true
			_emit_demo_sequence()
			get_viewport().set_input_as_handled()


func _build_ui() -> void:
	panel = PanelContainer.new()
	panel.position = Vector2(18, 18)
	panel.custom_minimum_size = Vector2(460, 0)
	add_child(panel)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 7)
	panel.add_child(box)
	var title := Label.new()
	title.text = "EXUS CONTROL - painel de integracao"
	title.add_theme_font_size_override("font_size", 19)
	box.add_child(title)
	status_label = Label.new()
	status_label.text = "CONTROL AUSENTE - aguardando UDP"
	box.add_child(status_label)
	integration_toggle = CheckButton.new()
	integration_toggle.text = "Integracao com Exus Control (UDP loopback)"
	integration_toggle.button_pressed = bool(ExusSettings.get_value("integration_enabled"))
	integration_toggle.toggled.connect(func(value: bool): ExusSettings.set_value("integration_enabled", value))
	box.add_child(integration_toggle)
	real_toggle = CheckButton.new()
	real_toggle.text = "Vibracoes reais (requer autorizacao no Control)"
	real_toggle.tooltip_text = "Desligado por padrao. Ativar aqui sozinho nunca aciona hardware."
	real_toggle.button_pressed = bool(ExusSettings.get_value("real_vibration_requested"))
	real_toggle.toggled.connect(func(value: bool): ExusSettings.set_value("real_vibration_requested", value))
	box.add_child(real_toggle)
	var help := Label.new()
	help.text = "F7: roteiro de teste | botoes: eventos manuais | F8: ocultar painel\nO Exus Control tambem precisa autorizar qualquer saida fisica."
	help.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	box.add_child(help)
	var buttons := GridContainer.new()
	buttons.columns = 3
	box.add_child(buttons)
	_add_button(buttons, "Disparo", func(): ExusEventBus.oneshot("weapon_fire", 0.45, 110, 0.0, "overlay_test"))
	_add_button(buttons, "Dano", func(): ExusEventBus.oneshot("damage", 0.70, 180, -90.0, "overlay_test"))
	_add_button(buttons, "Explosao", func(): ExusEventBus.oneshot("explosion", 0.95, 380, 180.0, "overlay_test"))
	_add_button(buttons, "Ameaca", func(): ExusEventBus.oneshot("threat", 0.60, 300, 180.0, "overlay_test"))
	_add_button(buttons, "Vento iniciar", func(): ExusEventBus.start_stream("wind", "overlay-wind", 0.35, 0.0, "overlay_test"))
	_add_button(buttons, "Vento parar", func(): ExusEventBus.stop_stream("wind", "overlay-wind", "overlay_test"))
	event_label = Label.new()
	event_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	event_label.text = "Ultimo evento: -"
	box.add_child(event_label)
	result_label = Label.new()
	result_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	result_label.text = "Ultimo resultado: -"
	box.add_child(result_label)
	panel.visible = bool(ExusSettings.get_value("show_debug_overlay"))


func _add_button(parent: GridContainer, text: String, action: Callable) -> void:
	var button := Button.new()
	button.text = text
	button.pressed.connect(action)
	parent.add_child(button)


func _emit_demo_sequence() -> void:
	ExusEventBus.start_stream("wind", "guided-wind", 0.35, 0.0, "guided_sequence")
	await get_tree().create_timer(1.0).timeout
	ExusEventBus.update_stream("wind", "guided-wind", 0.55, 20.0, "guided_sequence")
	await get_tree().create_timer(1.0).timeout
	ExusEventBus.oneshot("threat", 0.60, 280, 180.0, "guided_sequence")
	await get_tree().create_timer(1.0).timeout
	ExusEventBus.oneshot("explosion", 0.95, 400, 120.0, "guided_sequence")
	await get_tree().create_timer(1.0).timeout
	ExusEventBus.stop_stream("wind", "guided-wind", "guided_sequence")


func _on_event_emitted(payload: Dictionary) -> void:
	event_label.text = "Ultimo evento #%d: %s/%s (saida solicitada: %s)" % [payload.seq, payload.event, payload.state, payload.output_requested]


func _on_bridge_result(result: Dictionary) -> void:
	result_label.text = "Ultimo resultado #%s: %s" % [str(result.get("seq", "?")), str(result.get("result", "?"))]


func _on_delivery_state(state: String, detail: String) -> void:
	status_label.text = "%s - %s" % [state, detail]
