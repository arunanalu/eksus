"""Compatibilidade: a implementação passou para ``exus_control.ble_client``."""

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from exus_control.ble_client import *  # noqa: F401,F403
