import json
from pathlib import Path


def test_boat_profile_mirror_is_in_sync():
    root = Path(__file__).resolve().parents[2]
    game_profile = root / "game" / "boat-demo" / "config" / "haptics" / "boat-demo.v1.json"
    control_profile = root / "exus_control" / "profiles" / "boat-demo.v1.json"
    assert json.loads(game_profile.read_text(encoding="utf-8")) == json.loads(control_profile.read_text(encoding="utf-8"))
