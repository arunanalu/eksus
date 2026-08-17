from exus_control.events import SCHEMA, parse_game_event
from exus_control.haptic_mapper import HapticMapper


def boat_event(**changes):
    data = {
        "schema": SCHEMA, "session_id": "boat", "seq": 1, "sent_at_ms": 1,
        "event": "wind", "state": "update", "stream_id": "boat-wind",
        "azimuth_deg": -90.0, "magnitude": 0.8, "duration_ms": None,
        "source": "relative-wind", "haptic_profile": "boat-demo/v1",
        "output_requested": False,
    }
    data.update(changes)
    return parse_game_event(data)


def test_boat_wind_uses_independent_levels_for_left_forehead_right():
    left = HapticMapper().map(boat_event(azimuth_deg=-90.0), (0, 1, 2))
    front = HapticMapper().map(boat_event(azimuth_deg=0.0), (0, 1, 2))
    right = HapticMapper().map(boat_event(azimuth_deg=90.0), (0, 1, 2))
    assert left.command.startswith("stream 0x7 0:")
    assert front.command.startswith("stream 0x7 0:")
    assert right.command.startswith("stream 0x7 0:")
    assert left.zones == (0,)
    assert front.zones == (2,)
    assert right.zones == (1,)


def test_ice_collision_uses_short_high_priority_stream():
    intent = HapticMapper().map(boat_event(event="ice_collision", state="oneshot", stream_id=None, duration_ms=110), (0, 1, 2))
    assert " 110 30 95" in intent.command
