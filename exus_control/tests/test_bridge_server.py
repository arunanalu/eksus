import asyncio
import json

from exus_control.bridge_server import BridgeServer
from exus_control.events import SCHEMA
from exus_control.session import BridgeSession
from exus_control.transports import Capabilities, MockTransport


class CapturingTransport:
    def __init__(self): self.messages = []
    def sendto(self, data, addr): self.messages.append((json.loads(data.decode()), addr))


def valid_event(seq=1):
    return {"schema": SCHEMA, "session_id": "udp-game", "seq": seq, "sent_at_ms": 1,
            "event": "explosion", "state": "oneshot", "stream_id": None, "azimuth_deg": 0,
            "magnitude": .7, "duration_ms": 100, "source": "test", "output_requested": False}


def test_udp_handler_returns_simulated_result_and_never_binds_public_interface():
    async def scenario():
        mock = MockTransport(Capabilities((0, 1, 2)))
        server = BridgeServer(BridgeSession(mock, mock.capabilities))
        capture = CapturingTransport()
        await server._handle(json.dumps(valid_event()).encode(), ("127.0.0.1", 50000), capture)
        return server, capture
    server, capture = asyncio.run(scenario())
    reply, address = capture.messages[0]
    assert address == ("127.0.0.1", 50000)
    assert reply["result"] == "simulated"
    assert reply["command"].startswith("group 0x7 pulse")
    assert server.stats.simulated == 1


def test_udp_handler_rejects_bad_json_and_large_datagrams():
    async def scenario():
        server = BridgeServer(BridgeSession(MockTransport()))
        capture = CapturingTransport()
        await server._handle(b"{broken", ("127.0.0.1", 1), capture)
        await server._handle(b"x" * 2049, ("127.0.0.1", 1), capture)
        return capture
    capture = asyncio.run(scenario())
    assert [item[0]["reason"] for item in capture.messages] == ["invalid_json", "datagram_too_large"]


def test_real_loopback_udp_round_trip():
    class ClientProtocol(asyncio.DatagramProtocol):
        def __init__(self, future): self.future = future
        def datagram_received(self, data, _addr):
            if not self.future.done(): self.future.set_result(json.loads(data.decode()))

    async def scenario():
        mock = MockTransport(Capabilities((0,)))
        server = BridgeServer(BridgeSession(mock, mock.capabilities), port=0)
        await server.start()
        port = server._transport.get_extra_info("sockname")[1]
        response = asyncio.get_running_loop().create_future()
        client, _ = await asyncio.get_running_loop().create_datagram_endpoint(
            lambda: ClientProtocol(response), remote_addr=("127.0.0.1", port))
        client.sendto(json.dumps(valid_event()).encode())
        reply = await asyncio.wait_for(response, timeout=1)
        client.close(); await server.stop()
        return reply
    assert asyncio.run(scenario())["result"] == "simulated"


def test_refuses_non_loopback_binding():
    try:
        BridgeServer(BridgeSession(MockTransport()), host="0.0.0.0")
    except ValueError as exc:
        assert "127.0.0.1" in str(exc)
    else:
        raise AssertionError("deveria recusar interface pública")
