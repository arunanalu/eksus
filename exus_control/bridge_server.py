"""Servidor UDP exclusivamente loopback para a integração com jogos."""

from __future__ import annotations

import asyncio
import json
from dataclasses import dataclass, field
from typing import Callable

from .session import BridgeResult, BridgeSession

DEFAULT_HOST, DEFAULT_PORT, MAX_DATAGRAM_BYTES = "127.0.0.1", 4242, 2_048


@dataclass
class BridgeStats:
    received: int = 0
    simulated: int = 0
    sent: int = 0
    rejected: int = 0
    expired: int = 0
    dropped: int = 0
    last_event: str = "—"
    last_command: str = "—"

    def record(self, result: BridgeResult, event: str | None = None) -> None:
        self.received += 1
        if result.result in {"simulated", "sent", "rejected", "expired", "dropped"}:
            setattr(self, result.result, getattr(self, result.result) + 1)
        self.last_event = event or self.last_event
        self.last_command = result.command or self.last_command


class _Protocol(asyncio.DatagramProtocol):
    def __init__(self, server: "BridgeServer"):
        self.server = server
        self.transport: asyncio.DatagramTransport | None = None

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data: bytes, addr):
        asyncio.create_task(self.server._handle(data, addr, self.transport))

    def error_received(self, exc):
        self.server.last_error = str(exc)


class BridgeServer:
    def __init__(self, session: BridgeSession, *, host: str = DEFAULT_HOST, port: int = DEFAULT_PORT,
                 on_result: Callable[[BridgeResult], None] | None = None):
        if host != DEFAULT_HOST:
            raise ValueError("a ponte MVP aceita somente 127.0.0.1")
        self.session, self.host, self.port, self.on_result = session, host, port, on_result
        self.stats = BridgeStats()
        self.last_error = ""
        self._transport: asyncio.DatagramTransport | None = None
        self._ticker: asyncio.Task | None = None

    @property
    def listening(self) -> bool:
        return self._transport is not None

    async def start(self) -> None:
        if self._transport: return
        loop = asyncio.get_running_loop()
        transport, _ = await loop.create_datagram_endpoint(lambda: _Protocol(self), local_addr=(self.host, self.port))
        self._transport = transport
        self._ticker = asyncio.create_task(self._run_ticker())

    async def stop(self) -> None:
        if self._transport: self._transport.close()
        self._transport = None
        if self._ticker:
            self._ticker.cancel()
            try:
                await self._ticker
            except asyncio.CancelledError:
                pass
            self._ticker = None
        await self.session.stop_all()

    async def _handle(self, data: bytes, addr, transport: asyncio.DatagramTransport | None) -> None:
        if len(data) > MAX_DATAGRAM_BYTES:
            result = BridgeResult(None, None, "rejected", "disconnected", reason="datagram_too_large")
            event = None
        else:
            try:
                payload = json.loads(data.decode("utf-8"))
                result = await self.session.handle_payload(payload)
                event = payload.get("event") if isinstance(payload, dict) else None
            except (UnicodeDecodeError, json.JSONDecodeError):
                result, event = BridgeResult(None, None, "rejected", "disconnected", reason="invalid_json"), None
        self.stats.record(result, event)
        if self.on_result: self.on_result(result)
        if transport:
            transport.sendto(json.dumps(result.as_dict(), separators=(",", ":")).encode("utf-8"), addr)

    async def tick(self) -> int:
        expired = await self.session.expire_streams()
        self.stats.expired += expired
        return expired

    async def _run_ticker(self) -> None:
        while self.listening:
            await asyncio.sleep(0.1)
            await self.tick()
