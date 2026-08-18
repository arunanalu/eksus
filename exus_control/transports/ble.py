"""Adaptador do cliente BLE; é o único transporte que importa a biblioteca BLE."""

from __future__ import annotations

from collections.abc import Callable

from ..ble_client import ExusBleClient, ExusDevice, parse_capabilities
from .base import Capabilities, CommandResult, TransportState


class BleTransportAdapter:
    def __init__(self, device: ExusDevice, *, client_factory: Callable[[ExusDevice], ExusBleClient] = ExusBleClient):
        self.device = device
        self._factory = client_factory
        self.client: ExusBleClient | None = None
        self.capabilities: Capabilities | None = None

    @property
    def state(self) -> TransportState:
        return TransportState.CONNECTED if self.client and self.client.connected else TransportState.DISCONNECTED

    async def connect(self) -> Capabilities:
        self.client = self._factory(self.device)
        await self.client.connect()
        # O BLE sobe antes da calibracao dos atuadores; aguarde a consulta de
        # capacidades sem atrasar a descoberta/conexao inicial.
        reply = await self.client.command("Q 0", timeout=15.0)
        zones = tuple(parse_capabilities(reply))
        if not zones:
            await self.disconnect(); raise RuntimeError("O dispositivo não informou zonas prontas.")
        self.capabilities = Capabilities(zones, self.device.name)
        return self.capabilities

    async def info(self) -> str:
        if not self.client: raise RuntimeError("Conecte um protótipo primeiro.")
        return await self.client.info()

    async def send(self, command: str) -> CommandResult:
        if not self.client: raise RuntimeError("Conecte um protótipo primeiro.")
        response = await self.client.command(command)
        return CommandResult(response, acknowledged=response.startswith("A "))

    async def stop_all(self) -> None:
        await self.send("stop all")

    async def emergency(self) -> None:
        if not self.client: raise RuntimeError("Conecte um protótipo primeiro.")
        await self.client.emergency()

    async def disconnect(self) -> None:
        if self.client: await self.client.disconnect()
        self.client = None
