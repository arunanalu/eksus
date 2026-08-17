"""Traduz eventos de jogo para comandos lógicos, sem conhecer BLE ou I2C."""

from __future__ import annotations

from dataclasses import dataclass

from .events import GameEvent


@dataclass(frozen=True)
class HapticIntent:
    command: str
    zones: tuple[int, ...]
    priority: int
    ttl_ms: int
    signature: str
    stream_key: tuple[str, str] | None = None


class MappingError(ValueError):
    pass


class HapticMapper:
    """Mapeamento configurável; uma direção ausente é rejeitada explicitamente."""

    PRIORITIES = {"wind": 20, "weapon_fire": 30, "threat": 50, "damage": 90, "explosion": 100}

    def __init__(self, directional_zones: dict[str, int] | None = None):
        self.directional_zones = directional_zones or {"front": 0, "left": 1, "right": 2, "back": 3}

    @staticmethod
    def _direction(azimuth: float | None) -> str:
        if azimuth is None or -45 <= azimuth <= 45:
            return "front"
        if -135 < azimuth < -45:
            return "left"
        if 45 < azimuth < 135:
            return "right"
        return "back"

    def _zones(self, event: GameEvent, ready: tuple[int, ...]) -> tuple[int, ...]:
        if not ready:
            raise MappingError("nenhuma zona pronta")
        direction = self._direction(event.azimuth_deg)
        target = self.directional_zones.get(direction)
        if target in ready:
            return (target,)
        if len(ready) == 1:
            return (ready[0],)  # degradação de uma zona é intencional e observável no log.
        raise MappingError(f"zona configurada para {direction} não está pronta")

    @staticmethod
    def _command(zones: tuple[int, ...], intensity: int, duration: int, frequency: int) -> str:
        mask = sum(1 << zone for zone in zones)
        return f"group 0x{mask:X} pulse {intensity} {duration} {frequency}"

    def map(self, event: GameEvent, ready_zones: tuple[int, ...]) -> HapticIntent | None:
        if event.state == "stop":
            return None
        primary = self._zones(event, ready_zones)
        zones = primary
        if event.event == "explosion" and len(ready_zones) > 1:
            ordered = list(primary) + [zone for zone in ready_zones if zone not in primary]
            zones = tuple(ordered[:3])
        duration = event.duration_ms or (250 if event.event == "wind" else 100)
        base, spread, frequency = {
            "damage": (15, 35, 30), "explosion": (25, 25, 40), "wind": (5, 15, 12),
            "threat": (10, 25, 20), "weapon_fire": (8, 20, 45),
        }[event.event]
        intensity = max(1, min(50, round(base + spread * event.magnitude)))
        ttl = max(duration + 100, 500) if event.state == "oneshot" else 1_500
        stream_key = (event.session_id, event.stream_id) if event.stream_id else None
        return HapticIntent(
            command=self._command(zones, intensity, duration, frequency), zones=zones,
            priority=self.PRIORITIES[event.event], ttl_ms=ttl,
            signature=f"{event.event}:{event.state}:{zones}:{intensity}:{duration}:{frequency}", stream_key=stream_key,
        )
