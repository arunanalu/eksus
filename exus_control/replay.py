"""Replay seguro de logs: usa MockTransport e jamais arma saída física."""

from __future__ import annotations

import argparse
import asyncio
import json
from pathlib import Path

from .session import BridgeSession
from .transports import Capabilities, MockTransport


async def replay(path: Path) -> list[str]:
    transport = MockTransport(Capabilities((0, 1, 2, 3)))
    session = BridgeSession(transport, transport.capabilities)
    commands: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        row = json.loads(line)
        event = row.get("game_event")
        if isinstance(event, dict):
            result = await session.handle_payload(event)
            if result.command: commands.append(result.command)
    return commands


def main() -> None:
    parser = argparse.ArgumentParser(description="Replay seguro de uma sessão Exus")
    parser.add_argument("log", type=Path)
    for command in asyncio.run(replay(parser.parse_args().log)):
        print(f"WOULD_SEND {command}")


if __name__ == "__main__":
    main()
