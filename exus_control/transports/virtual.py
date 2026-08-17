from __future__ import annotations

from collections import deque

from .base import Capabilities, CommandResult, TransportState
from .mock import MockTransport


class VirtualExusTransport(MockTransport):
    """Dispositivo virtual com falhas reproduzíveis para a suíte ponta a ponta."""
    def __init__(self, capabilities: Capabilities = Capabilities((0,)), outcomes: list[str] | None = None):
        super().__init__(capabilities)
        self.outcomes = deque(outcomes or [])

    async def send(self, command: str) -> CommandResult:
        outcome = self.outcomes.popleft() if self.outcomes else "ack"
        if outcome == "disconnect":
            await self.disconnect(); raise ConnectionError("desconexão virtual")
        if outcome == "timeout":
            raise TimeoutError("timeout virtual")
        self.commands.append(command)
        if outcome == "nack":
            return CommandResult(f"N 0 comando rejeitado: {command}", acknowledged=False)
        return CommandResult(f"A 0 {command}")
