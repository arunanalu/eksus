import asyncio
from pathlib import Path

from exus_control.ble_client import ExusBleClient, ExusDevice, parse_capabilities


def test_capability_fixtures_are_parsed_without_ble():
    fixture_dir = Path(__file__).parents[1] / "fixtures"
    assert parse_capabilities((fixture_dir / "capabilities_q.txt").read_text()) == [0, 2, 7]
    assert (fixture_dir / "ack.txt").read_text().startswith("A ")
    assert (fixture_dir / "nack.txt").read_text().startswith("N ")
    assert parse_capabilities('A 1 {"zones_ready":[]}') == []


class FakeClient:
    def __init__(self, *_args, **_kwargs):
        self.is_connected = False
        self.notifications = {}
        self.writes = []
    async def connect(self): self.is_connected = True
    async def pair(self): return True
    async def start_notify(self, uuid, callback): self.notifications[uuid] = callback
    async def stop_notify(self, _uuid): pass
    async def disconnect(self): self.is_connected = False
    async def write_gatt_char(self, _uuid, data, response=True):
        self.writes.append((data, response))
        sequence = data.decode().split()[0][1:]
        next(iter(self.notifications.values()))(None, f"A {sequence} ok".encode())


def test_command_framing_and_ack_correlation_without_radio():
    async def scenario():
        client = ExusBleClient(ExusDevice("ignored", "Exus-test", -40, object()), client_factory=FakeClient)
        await client.connect()
        response = await client.command("pulse 0 15 500 10")
        return client, response
    client, response = asyncio.run(scenario())
    assert client._client.writes == [(b"@1 pulse 0 15 500 10\n", True)]
    assert response == "A 1 ok"
