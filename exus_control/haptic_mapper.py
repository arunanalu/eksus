"""Traduz eventos de jogo para comandos lógicos, sem conhecer BLE ou I2C."""

from __future__ import annotations

from dataclasses import dataclass
import math
import time

from .events import GameEvent
from .profile_loader import load_profile


@dataclass(frozen=True)
class HapticIntent:
    command: str
    zones: tuple[int, ...]
    priority: int
    ttl_ms: int
    signature: str
    stream_key: tuple[str, str] | None = None
    min_interval_ms: int = 0


class MappingError(ValueError):
    pass


class HapticMapper:
    """Mapeamento configurável; uma direção ausente é rejeitada explicitamente."""

    PRIORITIES = {"wind": 20, "weapon_fire": 30, "threat": 50, "damage": 90, "ice_collision": 95, "explosion": 100}

    def __init__(self, directional_zones: dict[str, int] | None = None, boat_profile: dict | None = None):
        self.directional_zones = directional_zones or {"front": 0, "left": 1, "right": 2, "back": 3}
        self.boat_profile = boat_profile or load_profile("boat-demo/v1")
        self._boat_wind_levels: dict[tuple[str, str], tuple[dict[int, float], float]] = {}

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
            if event.haptic_profile == "boat-demo/v1" and event.stream_id:
                self._boat_wind_levels.pop((event.session_id, event.stream_id), None)
            return None
        if event.haptic_profile == "boat-demo/v1":
            return self._map_boat_demo(event, ready_zones)
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

    @staticmethod
    def _boat_weight(azimuth: float, bearing: float, sharpness: float = 1.6) -> float:
        delta = math.radians((azimuth - bearing + 180.0) % 360.0 - 180.0)
        return max(0.0, math.cos(delta)) ** sharpness

    def _map_boat_demo(self, event: GameEvent, ready_zones: tuple[int, ...]) -> HapticIntent:
        if event.event not in {"wind", "ice_collision"}:
            raise MappingError(f"evento {event.event} não pertence ao perfil boat-demo/v1")
        if not ready_zones:
            raise MappingError("nenhuma zona pronta")
        azimuth = event.azimuth_deg if event.azimuth_deg is not None else 0.0
        zones = {int(zone): config for zone, config in self.boat_profile["zones"].items()}
        bearings = {zone: float(config["bearing_deg"]) for zone, config in zones.items()}
        gains = {zone: float(config["gain"]) for zone, config in zones.items()}
        if event.event == "wind":
            config = self.boat_profile["wind"]
            sharpness = float(config["direction_sharpness"])
            targets = {
                zone: ((float(zones[zone]["min_pct"]) +
                    (float(zones[zone]["max_pct"]) - float(zones[zone]["min_pct"])) * event.magnitude * self._boat_weight(azimuth, bearing, sharpness)) * gains[zone]
                    if self._boat_weight(azimuth, bearing, sharpness) > 0.04 else 0.0)
                for zone, bearing in bearings.items()
                if zone in ready_zones
            }
            stream_key = (event.session_id, event.stream_id or "boat-wind")
            previous, previous_at = self._boat_wind_levels.get(stream_key, ({zone: 0.0 for zone in zones}, 0.0))
            now = time.monotonic() * 1000.0
            alpha = 1.0 if previous_at == 0.0 else 1.0 - math.exp(-(now - previous_at) / float(config["smoothing_ms"]))
            levels = {zone: round(previous.get(zone, 0.0) + (target - previous.get(zone, 0.0)) * alpha) for zone, target in targets.items()}
            self._boat_wind_levels[stream_key] = ({zone: float(value) for zone, value in levels.items()}, now)
            duration, frequency, priority = int(config["ttl_ms"]), float(config["frequency_hz"]), self.PRIORITIES["wind"]
            min_interval_ms = round(1000.0 / float(config["command_hz"]))
        else:
            config = self.boat_profile["ice_collision"]
            levels = {
                zone: round((float(config["min_pct"]) +
                    (float(config["max_pct"]) - float(config["min_pct"])) * event.magnitude * self._boat_weight(azimuth, bearing, 1.2)) * gains[zone])
                for zone, bearing in bearings.items()
                if zone in ready_zones and self._boat_weight(azimuth, bearing, 1.2) > 0.12
            }
            duration = event.duration_ms or int(config["duration_ms"])
            frequency, priority = float(config["frequency_hz"]), self.PRIORITIES["ice_collision"]
            stream_key, min_interval_ms = None, 0
        if not levels:
            raise MappingError("nenhuma zona pronta para a direção solicitada")
        levels = {zone: max(0, min(50, value)) for zone, value in levels.items()}
        active_zones = tuple(sorted(zone for zone, value in levels.items() if value > 0))
        if not active_zones:
            raise MappingError("nenhuma zona pronta para a direÃ§Ã£o solicitada")
        mask = sum(1 << zone for zone in levels)
        encoded_levels = ",".join(f"{zone}:{value}" for zone, value in sorted(levels.items()))
        return HapticIntent(
            command=f"stream 0x{mask:X} {encoded_levels} {duration} {frequency:g} {priority}",
            zones=active_zones, priority=priority,
            ttl_ms=max(duration + 100, 700) if event.state == "oneshot" else duration,
            signature=f"boat:{event.event}:{event.state}:{encoded_levels}:{duration}:{frequency}",
            stream_key=stream_key,
            min_interval_ms=min_interval_ms,
        )
