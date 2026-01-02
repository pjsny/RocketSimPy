"""Tests for RLViser UDP communication functionality."""

import pytest
import RocketSim as rs


class TestRLViserModule:
    """Test that the rlviser submodule is properly exposed."""

    def test_submodule_exists(self):
        """Verify rlviser submodule is accessible."""
        assert hasattr(rs, "rlviser")

    def test_port_constants(self):
        """Verify port constants are defined."""
        assert rs.rlviser.RLVISER_PORT == 45243
        assert rs.rlviser.ROCKETSIM_PORT == 34254

    def test_packet_types(self):
        """Verify UdpPacketType enum values exist."""
        # Verify all packet types are defined
        assert rs.rlviser.UdpPacketType.QUIT is not None
        assert rs.rlviser.UdpPacketType.GAME_STATE is not None
        assert rs.rlviser.UdpPacketType.CONNECTION is not None
        assert rs.rlviser.UdpPacketType.PAUSED is not None
        assert rs.rlviser.UdpPacketType.SPEED is not None
        assert rs.rlviser.UdpPacketType.RENDER is not None


class TestGameState:
    """Test GameState serialization."""

    @pytest.fixture
    def arena(self):
        """Create a test arena with some cars."""
        rs.init("../collision_meshes")
        arena = rs.Arena(rs.GameMode.SOCCAR)
        arena.add_car(rs.Team.BLUE, rs.CarConfig(rs.CarConfig.OCTANE))
        arena.add_car(rs.Team.ORANGE, rs.CarConfig(rs.CarConfig.DOMINUS))
        return arena

    def test_game_state_from_arena(self, arena):
        """Test creating GameState from Arena."""
        state = rs.rlviser.GameState.from_arena(arena)

        assert state.tick_count == arena.tick_count
        assert state.tick_rate == arena.tick_rate
        assert state.game_mode == rs.GameMode.SOCCAR
        assert len(state.cars) == 2
        assert len(state.pads) == 34  # Standard soccar boost pad count

    def test_game_state_serialization(self, arena):
        """Test that GameState can be serialized to bytes."""
        state = rs.rlviser.GameState.from_arena(arena)
        data = state.to_bytes()

        # Should have at least the minimum header size
        assert len(data) >= 21  # MIN_NUM_BYTES

        # Verify we can deserialize
        restored = rs.rlviser.GameState.from_bytes(data)

        assert restored.tick_count == state.tick_count
        assert restored.tick_rate == state.tick_rate
        assert restored.game_mode == state.game_mode
        assert len(restored.cars) == len(state.cars)
        assert len(restored.pads) == len(state.pads)

    def test_game_state_car_info(self, arena):
        """Test that car info is properly captured."""
        # Set some car state
        cars = arena.get_cars()
        car = cars[0]
        car_state = car.get_state()
        car_state.pos = rs.Vec(100.0, 200.0, 50.0)
        car_state.boost = 75.0
        car.set_state(car_state)

        state = rs.rlviser.GameState.from_arena(arena)

        # Find the car in the state
        car_info = None
        for c in state.cars:
            if c.id == car.id:
                car_info = c
                break

        assert car_info is not None
        assert car_info.team == rs.Team.BLUE
        assert abs(car_info.state.pos.x - 100.0) < 0.01
        assert abs(car_info.state.pos.y - 200.0) < 0.01
        assert abs(car_info.state.boost - 75.0) < 0.01

    def test_game_state_ball_info(self, arena):
        """Test that ball info is properly captured."""
        # Set ball state
        ball = arena.ball
        ball_state = ball.get_state()
        ball_state.pos = rs.Vec(500.0, -300.0, 150.0)
        ball_state.vel = rs.Vec(1000.0, -500.0, 200.0)
        ball.set_state(ball_state)

        state = rs.rlviser.GameState.from_arena(arena)

        assert abs(state.ball.state.pos.x - 500.0) < 0.01
        assert abs(state.ball.state.pos.y - (-300.0)) < 0.01
        assert abs(state.ball.state.vel.x - 1000.0) < 0.01

    def test_game_state_boost_pads(self, arena):
        """Test that boost pad info is properly captured."""
        state = rs.rlviser.GameState.from_arena(arena)

        # All pads should start active
        for pad in state.pads:
            assert pad.is_active
            assert pad.cooldown == 0.0

        # Count big/small pads
        big_pads = sum(1 for p in state.pads if p.is_big)
        small_pads = sum(1 for p in state.pads if not p.is_big)

        assert big_pads == 6  # Standard soccar has 6 big pads
        assert small_pads == 28  # And 28 small pads


class TestSocketClass:
    """Test Socket class (without actual network operations)."""

    def test_socket_creation(self):
        """Test that Socket can be created."""
        socket = rs.rlviser.Socket()
        assert socket is not None
        assert not socket.is_connected()

    def test_socket_initial_state(self):
        """Test initial socket state."""
        socket = rs.rlviser.Socket()
        assert not socket.is_paused()
        assert socket.get_game_speed() == 1.0


class TestArenaIntegration:
    """Test Arena methods for RLViser integration."""

    @pytest.fixture
    def arena(self):
        """Create a test arena."""
        rs.init("../collision_meshes")
        arena = rs.Arena(rs.GameMode.SOCCAR)
        arena.add_car(rs.Team.BLUE, rs.CarConfig(rs.CarConfig.OCTANE))
        return arena

    def test_get_game_state_method(self, arena):
        """Test Arena.get_game_state() method."""
        state = arena.get_game_state()

        assert isinstance(state, rs.rlviser.GameState)
        assert state.tick_count == arena.tick_count
        assert len(state.cars) == 1

    def test_game_state_updates_after_step(self, arena):
        """Test that game state reflects simulation changes."""
        initial_state = arena.get_game_state()
        initial_tick = initial_state.tick_count

        arena.step(10)

        new_state = arena.get_game_state()
        assert new_state.tick_count == initial_tick + 10


class TestReturnMessage:
    """Test ReturnMessage class."""

    def test_empty_return_message(self):
        """Test that empty ReturnMessage has None values."""
        msg = rs.rlviser.ReturnMessage()
        assert msg.game_state is None
        assert msg.speed is None
        assert msg.paused is None


class TestGlobalFunctions:
    """Test global rlviser functions."""

    def test_is_connected_default(self):
        """Test default connection state."""
        # Should be false by default (no connection attempt)
        assert not rs.rlviser.is_connected()

    def test_is_paused_default(self):
        """Test default pause state."""
        assert not rs.rlviser.is_paused()

    def test_get_game_speed_default(self):
        """Test default game speed."""
        assert rs.rlviser.get_game_speed() == 1.0
