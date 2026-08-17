"""Contrato canônico e validação estrita dos eventos recebidos do jogo."""

from __future__ import annotations

from dataclasses import dataclass
import math
from typing import Any, Mapping

SCHEMA = "exus.game-event/1"
EVENTS = frozenset({"damage", "explosion", "wind", "threat", "weapon_fire", "ice_collision"})
STATES = frozenset({"oneshot", "start", "update", "stop"})
MAX_DURATION_MS = 2_000


class EventValidationError(ValueError):
    """Evento que não é seguro ou compatível com o contrato v1."""


@dataclass(frozen=True)
class GameEvent:
    session_id: str
    seq: int
    sent_at_ms: int
    event: str
    state: str
    stream_id: str | None
    azimuth_deg: float | None
    magnitude: float
    duration_ms: int | None
    source: str
    output_requested: bool
    haptic_profile: str = "default/v1"


def _fail(message: str) -> None:
    raise EventValidationError(message)


def _integer(payload: Mapping[str, Any], key: str, *, minimum: int = 0) -> int:
    value = payload.get(key)
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        _fail(f"{key} deve ser um inteiro maior ou igual a {minimum}")
    return value


def _finite_number(value: Any, key: str, minimum: float, maximum: float) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        _fail(f"{key} deve ser um número finito")
    result = float(value)
    if not minimum <= result <= maximum:
        _fail(f"{key} deve estar entre {minimum} e {maximum}")
    return result


def parse_game_event(payload: Mapping[str, Any]) -> GameEvent:
    """Valida um documento JSON já decodificado, sem aceitar coerções implícitas."""
    if not isinstance(payload, Mapping):
        _fail("o evento deve ser um objeto JSON")
    if payload.get("schema") != SCHEMA:
        _fail("schema desconhecido")
    session_id = payload.get("session_id")
    if not isinstance(session_id, str) or not session_id.strip() or len(session_id) > 128:
        _fail("session_id inválido")
    event, state = payload.get("event"), payload.get("state")
    if event not in EVENTS:
        _fail("event desconhecido")
    if state not in STATES:
        _fail("state desconhecido")
    stream_id = payload.get("stream_id")
    if state in {"start", "update", "stop"}:
        if not isinstance(stream_id, str) or not stream_id.strip() or len(stream_id) > 128:
            _fail("stream_id é obrigatório para estados contínuos")
    elif stream_id is not None:
        _fail("stream_id deve ser null em oneshot")
    azimuth = payload.get("azimuth_deg")
    if azimuth is not None:
        azimuth = _finite_number(azimuth, "azimuth_deg", -180.0, 180.0)
    magnitude = _finite_number(payload.get("magnitude"), "magnitude", 0.0, 1.0)
    duration = payload.get("duration_ms")
    if state == "oneshot":
        duration = _integer(payload, "duration_ms", minimum=1)
        if duration > MAX_DURATION_MS:
            _fail(f"duration_ms não pode exceder {MAX_DURATION_MS}")
    elif duration is not None:
        duration = _integer(payload, "duration_ms", minimum=1)
        if duration > MAX_DURATION_MS:
            _fail(f"duration_ms não pode exceder {MAX_DURATION_MS}")
    source = payload.get("source", "unknown")
    if not isinstance(source, str) or not source.strip() or len(source) > 128:
        _fail("source inválido")
    output_requested = payload.get("output_requested", False)
    if not isinstance(output_requested, bool):
        _fail("output_requested deve ser booleano")
    haptic_profile = payload.get("haptic_profile", "default/v1")
    if not isinstance(haptic_profile, str) or not haptic_profile.strip() or len(haptic_profile) > 128:
        _fail("haptic_profile inválido")
    return GameEvent(
        session_id=session_id,
        seq=_integer(payload, "seq"),
        sent_at_ms=_integer(payload, "sent_at_ms"),
        event=event,
        state=state,
        stream_id=stream_id,
        azimuth_deg=azimuth,
        magnitude=magnitude,
        duration_ms=duration,
        source=source,
        output_requested=output_requested,
        haptic_profile=haptic_profile,
    )
