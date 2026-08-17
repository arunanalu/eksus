from __future__ import annotations

from .base import Capabilities, CommandResult, TransportState


class MockTransport:
    """Transporte de teste que preserva exatamente os comandos a serem enviados."""
    def __init__(self, capabilities: Capabilities = Capabilities((0,)), *, connected: bool = True):
        self.capabilities = capabilities
        self.commands: list[str] = []
        self._state = TransportState.CONNECTED if connected else TransportState.DISCONNECTED

    @property
    def state(self) -> TransportState:
        return self._state

    async def connect(self) -> Capabilities:
        self._state = TransportState.CONNECTED
        return self.capabilities

    async def send(self, command: str) -> CommandResult:
        if self._state != TransportState.CONNECTED:
            raise RuntimeError("transporte desconectado")
        self.commands.append(command)
        return CommandResult(f"A 0 {command}")

    async def stop_all(self) -> None:
        if self._state == TransportState.CONNECTED:
            self.commands.append("stop all")

    async def emergency(self) -> None:
        if self._state != TransportState.DISCONNECTED:
            self.commands.append("emergency")
            self._state = TransportState.EMERGENCY

    async def disconnect(self) -> None:
        self._state = TransportState.DISCONNECTED
