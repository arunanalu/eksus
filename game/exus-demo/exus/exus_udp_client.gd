class_name ExusUdpClient
extends Node

## Cliente sem bloqueio para a ponte loopback. Falhas alteram somente o overlay.
signal bridge_result_received(result: Dictionary)
signal request_timed_out(seq: int)
signal socket_error(message: String)

const RESPONSE_TIMEOUT_MS := 750

var peer: PacketPeerUDP
var pending: Dictionary = {}
var endpoint := ""


func send_event(payload: Dictionary, host: String, port: int) -> void:
	var new_endpoint := "%s:%d" % [host, port]
	if peer == null or endpoint != new_endpoint:
		peer = PacketPeerUDP.new()
		endpoint = new_endpoint
		var bind_error := peer.bind(0, "127.0.0.1")
		if bind_error != OK:
			socket_error.emit("Nao foi possivel reservar UDP local: %s" % error_string(bind_error))
			return
		var connection_error := peer.connect_to_host(host, port)
		if connection_error != OK:
			socket_error.emit("Nao foi possivel abrir UDP: %s" % error_string(connection_error))
			return
	var send_error := peer.put_packet(JSON.stringify(payload).to_utf8_buffer())
	if send_error != OK:
		socket_error.emit("Falha ao enviar UDP: %s" % error_string(send_error))
		return
	pending[payload.seq] = Time.get_ticks_msec() + RESPONSE_TIMEOUT_MS


func _process(_delta: float) -> void:
	if peer != null:
		while peer.get_available_packet_count() > 0:
			var packet: PackedByteArray = peer.get_packet()
			var decoded: Variant = JSON.parse_string(packet.get_string_from_utf8())
			if decoded is Dictionary and decoded.get("schema") == "exus.bridge-result/1":
				pending.erase(decoded.get("seq"))
				bridge_result_received.emit(decoded)
			else:
				socket_error.emit("Resposta UDP invalida")
	var now := Time.get_ticks_msec()
	for seq in pending.keys().duplicate():
		if now >= pending[seq]:
			pending.erase(seq)
			request_timed_out.emit(seq)
