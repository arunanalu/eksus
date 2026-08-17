from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Protocol


class TransportState(str, Enum):
    DISCONNECTED = "disconnected"
    CONNECTED = "connected"
    EMERGENCY = "emergency"


@dataclass(frozen=True)
class Capabilities:
    zones_ready: tuple[int, ...]
    device_name: str = "Exus"


@dataclass(frozen=True)
class CommandResult:
    response: str
    acknowledged: bool = True


class HapticTransport(Protocol):
    @property
    def state(self) -> TransportState: ...
    async def connect(self) -> Capabilities: ...
    async def send(self, command: str) -> CommandResult: ...
    async def stop_all(self) -> None: ...
    async def emergency(self) -> None: ...
    async def disconnect(self) -> None: ...
