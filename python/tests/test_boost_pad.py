"""Tests for the BoostPad class."""

import pytest  # noqa: F401
import RocketSim as rs


class TestBoostPadProperties:
    """Test BoostPad properties."""

    def test_boost_pad_has_position(self, arena):
        pads = arena.get_boost_pads()
        pad = pads[0]

        pos = pad.get_pos()
        assert isinstance(pos, rs.Vec)

    def test_boost_pad_is_big(self, arena):
        pads = arena.get_boost_pads()

        # Find a big and small pad
        big_pads = [p for p in pads if p.is_big]
        small_pads = [p for p in pads if not p.is_big]

        assert len(big_pads) > 0
        assert len(small_pads) > 0


class TestBoostPadState:
    """Test BoostPad state management."""

    def test_get_state(self, arena):
        pad = arena.get_boost_pads()[0]
        state = pad.get_state()

        assert hasattr(state, "is_active")
        assert hasattr(state, "cooldown")

    def test_pad_starts_active(self, arena):
        pad = arena.get_boost_pads()[0]
        state = pad.get_state()

        assert state.is_active
        assert state.cooldown == 0.0

    def test_set_state(self, arena):
        pad = arena.get_boost_pads()[0]

        state = rs.BoostPadState()
        state.is_active = False
        state.cooldown = 5.0

        pad.set_state(state)

        new_state = pad.get_state()
        assert not new_state.is_active
        assert new_state.cooldown == 5.0
