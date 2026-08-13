"""Biblioteca assíncrona compartilhada pelos clientes BLE do Exus."""

from __future__ import annotations

import asyncio
import itertools
from dataclasses import dataclass

from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError

SERVICE_UUID = "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10001"
COMMAND_UUID = "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10002"
RESPONSE_UUID = "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10003"
STATUS_UUID = "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10004"
DEVICE_INFO_UUID = "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10005"
EMERGENCY_UUID = "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10006"


@dataclass(frozen=True)
class ExusDevice:
    address: str
    name: str
    rssi: int
    device: object


def _name(device, advertisement_data=None) -> str:
    if advertisement_data and advertisement_data.local_name:
        return advertisement_data.local_name
    return device.name or ""


async def scan(timeout: float = 5.0) -> list[ExusDevice]:
    found = await BleakScanner.discover(timeout=timeout, return_adv=True)
    devices = []
    for address, (device, advertisement) in found.items():
        name = _name(device, advertisement)
        service_uuids = {str(value).lower() for value in (advertisement.service_uuids or [])}
        if name.startswith("Exus-") or SERVICE_UUID in service_uuids:
            devices.append(ExusDevice(address, name or "Exus sem nome", advertisement.rssi, device))
    return sorted(devices, key=lambda item: item.name)


async def find(identifier: str) -> ExusDevice:
    needle = identifier.lower()
    for device in await scan():
        if needle in device.address.lower() or needle in device.name.lower():
            return device
    raise RuntimeError(f"Nenhum Exus encontrado para '{identifier}'. Procure novamente.")


class ExusBleClient:
    """Cliente de uma única conexão, com respostas correlacionadas por sequência."""

    def __init__(self, device: ExusDevice):
        self.device = device
        self._client = BleakClient(device.device, timeout=20.0)
        self._sequences = itertools.count(1)
        self._responses: dict[int, asyncio.Future[str]] = {}
        self.status = ""
        self.disconnected = asyncio.Event()

    @property
    def connected(self) -> bool:
        return self._client.is_connected

    async def connect(self, pair: bool = True) -> None:
        self.disconnected.clear()
        await self._client.connect()
        if pair:
            try:
                paired = await self._client.pair()
                if paired is False:
                    raise BleakError("o Windows recusou o pareamento")
            except BleakError as exc:
                if "already" not in str(exc).lower():
                    await self.disconnect()
                    raise
        await self._client.start_notify(RESPONSE_UUID, self._received_response)
        await self._client.start_notify(STATUS_UUID, self._received_status)

    async def disconnect(self) -> None:
        for uuid in (RESPONSE_UUID, STATUS_UUID):
            try:
                await self._client.stop_notify(uuid)
            except BleakError:
                pass
        if self._client.is_connected:
            await self._client.disconnect()
        self.disconnected.set()

    def _received_response(self, _, data: bytearray) -> None:
        text = bytes(data).decode("utf-8", errors="replace").strip()
        pieces = text.split(maxsplit=2)
        if len(pieces) < 2 or pieces[0] not in {"A", "N"}:
            return
        try:
            sequence = int(pieces[1])
        except ValueError:
            return
        future = self._responses.pop(sequence, None)
        if future and not future.done():
            future.set_result(text)

    def _received_status(self, _, data: bytearray) -> None:
        self.status = bytes(data).decode("utf-8", errors="replace").strip()

    async def info(self) -> str:
        raw = await self._client.read_gatt_char(DEVICE_INFO_UUID)
        return bytes(raw).decode("utf-8", errors="replace")

    async def command(self, command: str, timeout: float = 5.0) -> str:
        if not self.connected:
            raise RuntimeError("O Exus não está conectado.")
        sequence = next(self._sequences)
        response = asyncio.get_running_loop().create_future()
        self._responses[sequence] = response
        payload = f"@{sequence} {command.strip()}\n".encode("utf-8")
        try:
            await self._client.write_gatt_char(COMMAND_UUID, payload, response=True)
            return await asyncio.wait_for(response, timeout=timeout)
        finally:
            self._responses.pop(sequence, None)

    async def emergency(self) -> None:
        if not self.connected:
            raise RuntimeError("O Exus não está conectado.")
        await self._client.write_gatt_char(EMERGENCY_UUID, b"stop", response=True)


def parse_capabilities(reply: str) -> list[int]:
    """Extrai `zones_ready` da resposta atual `Q`, sem tornar JSON do firmware uma API frágil."""
    marker = '"zones_ready":['
    start = reply.find(marker)
    if start < 0:
        return []
    end = reply.find("]", start)
    if end < 0:
        return []
    raw = reply[start + len(marker):end].strip()
    if not raw:
        return []
    try:
        return [int(value) for value in raw.split(",")]
    except ValueError:
        return []
