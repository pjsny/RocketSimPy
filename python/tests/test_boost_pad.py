"""Tests for the BoostPad class."""

import pytest  # noqa: F401
import RocketSim as rs


class TestBoostPadConfig:
    """Test BoostPadConfig class."""

    def test_default_constructor(self):
        """Test default BoostPadConfig constructor."""
        config = rs.BoostPadConfig()
        assert config.pos.x == 0
        assert config.pos.y == 0
        assert config.pos.z == 0
        assert config.is_big == False

    def test_constructor_with_pos(self):
        """Test BoostPadConfig constructor with position."""
        config = rs.BoostPadConfig(pos=rs.Vec(100, 200, 73))
        assert config.pos.x == 100
        assert config.pos.y == 200
        assert config.pos.z == 73
        assert config.is_big == False

    def test_constructor_with_is_big(self):
        """Test BoostPadConfig constructor with is_big."""
        config = rs.BoostPadConfig(is_big=True)
        assert config.is_big == True

    def test_constructor_with_all_args(self):
        """Test BoostPadConfig constructor with all arguments."""
        config = rs.BoostPadConfig(pos=rs.Vec(-2000, -2000, 73), is_big=True)
        assert config.pos.x == -2000
        assert config.pos.y == -2000
        assert config.pos.z == 73
        assert config.is_big == True

    def test_repr(self):
        """Test BoostPadConfig repr."""
        config = rs.BoostPadConfig(pos=rs.Vec(100, 200, 73), is_big=True)
        repr_str = repr(config)
        assert "BoostPadConfig" in repr_str
        assert "True" in repr_str


class TestCustomBoostPads:
    """Test custom boost pads feature."""

    def test_arena_with_custom_boost_pads(self):
        """Test creating arena with custom boost pads."""
        custom_pads = [
            rs.BoostPadConfig(pos=rs.Vec(-2000, -2000, 73), is_big=True),
            rs.BoostPadConfig(pos=rs.Vec(2000, 2000, 73), is_big=False),
        ]
        
        arena = rs.Arena(rs.GameMode.SOCCAR, custom_boost_pads=custom_pads)
        pads = arena.get_boost_pads()
        
        assert len(pads) == 2
        
        # Check positions match
        pad_positions = [(p.get_pos().x, p.get_pos().y) for p in pads]
        assert (-2000, -2000) in pad_positions
        assert (2000, 2000) in pad_positions
        
        # Check is_big matches
        big_pads = [p for p in pads if p.is_big]
        small_pads = [p for p in pads if not p.is_big]
        assert len(big_pads) == 1
        assert len(small_pads) == 1

    def test_custom_boost_pads_empty_list(self):
        """Test that empty custom boost pads list uses default pads."""
        arena = rs.Arena(rs.GameMode.SOCCAR, custom_boost_pads=[])
        pads = arena.get_boost_pads()
        
        # Should use default SOCCAR pads (34 total)
        assert len(pads) == 34

    def test_custom_boost_pads_gym_state(self):
        """Test gym state with custom boost pads."""
        custom_pads = [
            rs.BoostPadConfig(pos=rs.Vec(0, 0, 73), is_big=True),
        ]
        
        arena = rs.Arena(rs.GameMode.SOCCAR, custom_boost_pads=custom_pads)
        state = arena.get_gym_state()
        
        # Should have 1 pad state
        assert len(state["pads"]) == 1

    def test_custom_boost_pads_pickup(self):
        """Test that custom boost pads can be picked up."""
        custom_pads = [
            rs.BoostPadConfig(pos=rs.Vec(0, 0, 73), is_big=True),
        ]
        
        arena = rs.Arena(rs.GameMode.SOCCAR, custom_boost_pads=custom_pads)
        car = arena.add_car(rs.Team.BLUE, rs.CAR_CONFIG_OCTANE)
        
        # Position car at the boost pad
        state = car.get_state()
        state.pos = rs.Vec(0, 0, 17)
        state.vel = rs.Vec(0, 0, 0)
        state.boost = 0  # Empty boost
        car.set_state(state)
        
        # Step simulation
        arena.step(10)
        
        # Car should have picked up boost
        new_state = car.get_state()
        assert new_state.boost > 0


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
