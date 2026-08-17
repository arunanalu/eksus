import asyncio

from exus_control.events import SCHEMA
from exus_control.session import BridgeSession
from exus_control.transports import Capabilities, MockTransport, VirtualExusTransport


def payload(**changes):
    data = {"schema": SCHEMA, "session_id": "game-a", "seq": 1, "sent_at_ms": 10,
            "event": "damage", "state": "oneshot", "stream_id": None, "azimuth_deg": 0,
            "magnitude": .5, "duration_ms": 80, "source": "test", "output_requested": True}
    data.update(changes)
    return data


def run(coro): return asyncio.run(coro)


def test_simulated_by_default_never_replays_after_enabling_output():
    transport = MockTransport(Capabilities((0,)))
    session = BridgeSession(transport, transport.capabilities)
    result = run(session.handle_payload(payload()))
    assert result.result == "simulated"
    assert transport.commands == []
    assert run(session.set_hardware_output(True)) is True
    assert transport.commands == []
    sent = run(session.handle_payload(payload(seq=2)))
    assert sent.result == "sent"
    assert len(transport.commands) == 1


def test_double_authorization_requires_game_flag():
    transport = MockTransport(Capabilities((0,)))
    session = BridgeSession(transport, transport.capabilities)
    run(session.set_hardware_output(True))
    result = run(session.handle_payload(payload(output_requested=False)))
    assert result.result == "simulated"
    assert result.reason == "game_output_not_requested"
    assert transport.commands == []


def test_duplicate_and_direction_without_ready_zone_are_rejected():
    transport = MockTransport(Capabilities((0, 1)))
    session = BridgeSession(transport, transport.capabilities)
    first = run(session.handle_payload(payload(azimuth_deg=130)))
    assert first.result == "rejected"
    duplicate = run(session.handle_payload(payload(seq=1)))
    assert duplicate.reason == "duplicate_or_out_of_order"


def test_disable_disconnect_and_emergency_clear_and_stop():
    transport = MockTransport(Capabilities((0,)))
    session = BridgeSession(transport, transport.capabilities)
    run(session.set_hardware_output(True))
    run(session.set_hardware_output(False))
    assert transport.commands == ["stop all"]
    run(session.set_hardware_output(True))
    run(session.emergency())
    assert transport.commands[-1] == "emergency"
    assert session.hardware_output_enabled is False


def test_virtual_nack_timeout_and_disconnect_are_observable():
    async def scenario():
        transport = VirtualExusTransport(Capabilities((0,)), ["nack", "timeout", "disconnect"])
        session = BridgeSession(transport, transport.capabilities)
        await session.set_hardware_output(True)
        return [
            await session.handle_payload(payload(seq=1, magnitude=.4)),
            await session.handle_payload(payload(seq=2, magnitude=.5)),
            await session.handle_payload(payload(seq=3, magnitude=.6)),
            ]
    results = run(scenario())
    assert [item.result for item in results] == ["rejected", "simulated", "simulated"]
    assert results[0].reason == "firmware_nack"
    assert results[1].reason == "transport_timeout"
    assert results[2].reason.startswith("transport_error")


def test_ten_thousand_events_do_not_accumulate_a_queue():
    async def scenario():
        transport = MockTransport(Capabilities((0,)))
        session = BridgeSession(transport, transport.capabilities)
        for seq in range(10_000):
            await session.handle_payload(payload(seq=seq, output_requested=False, magnitude=(seq % 10) / 10))
        return session.arbiter._pending
    assert run(scenario()) == 0
