"""Log JSON Lines de sessão sem dados pessoais ou identificadores BLE."""

from __future__ import annotations

import json
from pathlib import Path
import time
from typing import Any


class SessionLogger:
    def __init__(self, path: Path | None = None):
        self.path = path
        self.records: list[dict[str, Any]] = []

    def write(self, **record: Any) -> dict[str, Any]:
        row = {"wall_time_ms": int(time.time() * 1000), "monotonic_ms": int(time.monotonic() * 1000), **record}
        self.records.append(row)
        if self.path:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            with self.path.open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(row, ensure_ascii=False, separators=(",", ":")) + "\n")
        return row
