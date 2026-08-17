"""Arbitragem limitada: prioridade, coalescência, TTL e streams contínuos."""

from __future__ import annotations

from dataclasses import dataclass

from .haptic_mapper import HapticIntent


@dataclass(frozen=True)
class ArbitrationResult:
    status: str
    reason: str = ""


class Arbiter:
    def __init__(self, *, max_queue: int = 128, cooldown_ms: int = 40, coalesce_ms: int = 80):
        self.max_queue, self.cooldown_ms, self.coalesce_ms = max_queue, cooldown_ms, coalesce_ms
        self._active: dict[int, tuple[int, int]] = {}
        self._last: dict[str, int] = {}
        self._streams: dict[tuple[str, str], tuple[HapticIntent, int]] = {}
        self._last_stream_sent: dict[tuple[str, str], int] = {}
        self._pending = 0

    def clear(self) -> None:
        self._active.clear(); self._last.clear(); self._streams.clear(); self._last_stream_sent.clear(); self._pending = 0

    def stop_stream(self, key: tuple[str, str] | None) -> bool:
        if key:
            self._last_stream_sent.pop(key, None)
        return bool(key and self._streams.pop(key, None))

    def stream_intent(self, key: tuple[str, str] | None) -> HapticIntent | None:
        record = self._streams.get(key) if key else None
        return record[0] if record else None

    def expire(self, now_ms: int) -> list[HapticIntent]:
        expired = [intent for intent, deadline in self._streams.values() if deadline <= now_ms]
        self._streams = {key: value for key, value in self._streams.items() if value[1] > now_ms}
        return expired

    def admit(self, intent: HapticIntent, now_ms: int) -> ArbitrationResult:
        if self._pending >= self.max_queue:
            return ArbitrationResult("dropped", "queue_full")
        if intent.stream_key and intent.min_interval_ms:
            last_stream = self._last_stream_sent.get(intent.stream_key)
            if last_stream is not None and now_ms - last_stream < intent.min_interval_ms:
                return ArbitrationResult("dropped", "stream_rate_limited")
        previous = self._last.get(intent.signature)
        if previous is not None and now_ms - previous < self.coalesce_ms:
            return ArbitrationResult("dropped", "coalesced")
        for zone in intent.zones:
            active = self._active.get(zone)
            if active and active[1] > now_ms and active[0] > intent.priority:
                return ArbitrationResult("dropped", "preempted_by_higher_priority")
        self._pending += 1
        try:
            self._last[intent.signature] = now_ms
            deadline = now_ms + intent.ttl_ms
            for zone in intent.zones:
                self._active[zone] = (intent.priority, deadline)
            if intent.stream_key:
                self._streams[intent.stream_key] = (intent, deadline)
                self._last_stream_sent[intent.stream_key] = now_ms
            return ArbitrationResult("accepted")
        finally:
            self._pending -= 1
