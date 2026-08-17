import math

import pytest

from exus_control.events import EventValidationError, SCHEMA, parse_game_event


def payload(**changes):
    event = {
        "schema": SCHEMA, "session_id": "game-a", "seq": 1, "sent_at_ms": 42,
        "event": "damage", "state": "oneshot", "stream_id": None, "azimuth_deg": -65.0,
        "magnitude": .42, "duration_ms": 80, "source": "projectile", "output_requested": False,
    }
    event.update(changes)
    return event


def test_parses_canonical_event():
    event = parse_game_event(payload())
    assert event.event == "damage"
    assert event.azimuth_deg == -65.0


@pytest.mark.parametrize("changes", [
    {"schema": "old"}, {"magnitude": math.nan}, {"magnitude": 1.1}, {"seq": True},
    {"duration_ms": 2_001}, {"output_requested": "true"}, {"state": "start", "stream_id": None},
    {"state": "oneshot", "stream_id": "not-allowed"},
])
def test_rejects_invalid_events(changes):
    with pytest.raises(EventValidationError):
        parse_game_event(payload(**changes))
