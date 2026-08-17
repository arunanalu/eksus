"""Sessão da ponte: dupla autorização, deduplicação e despacho ao transporte."""

from __future__ import annotations

from dataclasses import asdict, dataclass
import time
from typing import Mapping

from .arbitration import Arbiter
from .events import EventValidationError, GameEvent, parse_game_event
from .haptic_mapper import HapticMapper, MappingError
from .logging import SessionLogger
from .transports.base import Capabilities, HapticTransport, TransportState


@dataclass(frozen=True)
class BridgeResult:
    session_id: str | None
    seq: int | None
    result: str
    device: str
    command: str | None = None
    reason: str | None = None

    def as_dict(self) -> dict:
        return {"schema": "exus.bridge-result/1", **{key: value for key, value in asdict(self).items() if value is not None}}


class BridgeSession:
    def __init__(self, transport: HapticTransport, capabilities: Capabilities | None = None, *, mapper: HapticMapper | None = None,
                 logger: SessionLogger | None = None, clock_ms=None):
        self.transport, self.capabilities = transport, capabilities or Capabilities(())
        self.mapper, self.arbiter, self.logger = mapper or HapticMapper(), Arbiter(), logger or SessionLogger()
        self.hardware_output_enabled = False
        self._last_seq: dict[str, int] = {}
        self._clock_ms = clock_ms or (lambda: int(time.monotonic() * 1000))

    @property
    def connected(self) -> bool:
        return self.transport.state == TransportState.CONNECTED

    async def set_transport(self, transport: HapticTransport, capabilities: Capabilities) -> None:
        self.transport, self.capabilities = transport, capabilities
        self.hardware_output_enabled = False
        self.arbiter.clear()

    async def set_hardware_output(self, enabled: bool) -> bool:
        if not enabled:
            if self.connected: await self.transport.stop_all()
            self.hardware_output_enabled = False; self.arbiter.clear(); return True
        if not self.connected or not self.capabilities.zones_ready:
            self.hardware_output_enabled = False; return False
        self.hardware_output_enabled = True; return True

    async def on_disconnect(self) -> None:
        self.hardware_output_enabled = False
        self.arbiter.clear()

    async def stop_all(self) -> None:
        self.arbiter.clear()
        if self.connected: await self.transport.stop_all()

    async def emergency(self) -> None:
        self.arbiter.clear()
        if self.connected: await self.transport.emergency()
        self.hardware_output_enabled = False

    def _device(self) -> str:
        return "connected" if self.connected else "disconnected"

    async def handle_payload(self, payload: Mapping) -> BridgeResult:
        try:
            event = parse_game_event(payload)
        except EventValidationError as exc:
            result = BridgeResult(payload.get("session_id") if isinstance(payload, Mapping) else None,
                                  payload.get("seq") if isinstance(payload, Mapping) and isinstance(payload.get("seq"), int) else None,
                                  "rejected", self._device(), reason=str(exc))
            self.logger.write(**result.as_dict()); return result
        return await self.handle_event(event)

    async def handle_event(self, event: GameEvent) -> BridgeResult:
        now = self._clock_ms()
        old = self._last_seq.get(event.session_id)
        if old is not None and event.seq <= old:
            return self._record(event, "rejected", reason="duplicate_or_out_of_order")
        self._last_seq[event.session_id] = event.seq
        if event.state == "stop":
            stopped = self.arbiter.stop_stream((event.session_id, event.stream_id))
            if stopped and self.connected and self.hardware_output_enabled:
                await self.transport.stop_all()
            return self._record(event, "sent" if stopped and self.hardware_output_enabled else "simulated", command="stop all" if stopped else None,
                                reason=None if stopped else "unknown_stream")
        try:
            intent = self.mapper.map(event, self.capabilities.zones_ready)
        except MappingError as exc:
            return self._record(event, "rejected", reason=str(exc))
        assert intent is not None
        # Simulações percorrem validação e mapeamento, mas nunca ocupam uma fila
        # nem um cooldown que possa disparar após o operador habilitar hardware.
        if not (event.output_requested and self.hardware_output_enabled and self.connected):
            reason = "game_output_not_requested" if not event.output_requested else (
                "hardware_output_disabled" if not self.hardware_output_enabled else "device_disconnected")
            return self._record(event, "simulated", command=intent.command, reason=reason)
        arbitration = self.arbiter.admit(intent, now)
        if arbitration.status != "accepted":
            return self._record(event, arbitration.status, command=intent.command, reason=arbitration.reason)
        try:
            response = await self.transport.send(intent.command)
        except TimeoutError:
            return self._record(event, "simulated", command=intent.command, reason="transport_timeout")
        except (ConnectionError, RuntimeError) as exc:
            await self.on_disconnect()
            return self._record(event, "simulated", command=intent.command, reason=f"transport_error:{exc}")
        if not response.acknowledged:
            return self._record(event, "rejected", command=intent.command, reason="firmware_nack")
        return self._record(event, "sent", command=intent.command)

    async def expire_streams(self) -> int:
        expired = self.arbiter.expire(self._clock_ms())
        if expired and self.connected and self.hardware_output_enabled:
            await self.transport.stop_all()
        return len(expired)

    def _record(self, event: GameEvent, result: str, *, command: str | None = None, reason: str | None = None) -> BridgeResult:
        outcome = BridgeResult(event.session_id, event.seq, result, self._device(), command, reason)
        self.logger.write(**outcome.as_dict(), event=event.event, source=event.source, output_requested=event.output_requested,
                          game_event={"schema": "exus.game-event/1", "session_id": event.session_id, "seq": event.seq,
                                      "sent_at_ms": event.sent_at_ms, "event": event.event, "state": event.state,
                                      "stream_id": event.stream_id, "azimuth_deg": event.azimuth_deg,
                                      "magnitude": event.magnitude, "duration_ms": event.duration_ms,
                                      "source": event.source, "output_requested": event.output_requested})
        return outcome
