"""Tests for the Arena class."""

import pytest  # noqa: F401
import RocketSim as rs


class TestArenaCreation:
    """Test Arena creation."""

    def test_create_soccar_arena(self):
        arena = rs.Arena(rs.GameMode.SOCCAR)
        assert arena.game_mode == rs.GameMode.SOCCAR
        assert arena.tick_count == 0
        assert abs(arena.tick_rate - 120.0) < 0.01  # Float comparison

    def test_custom_tick_rate(self):
        arena = rs.Arena(rs.GameMode.SOCCAR, tick_rate=60.0)
        assert abs(arena.tick_rate - 60.0) < 0.01  # Float comparison


class TestArenaStep:
    """Test Arena simulation stepping."""

    def test_step_increases_tick_count(self, arena):
        initial_tick = arena.tick_count
        arena.step(1)
        assert arena.tick_count == initial_tick + 1

    def test_step_multiple_ticks(self, arena):
        initial_tick = arena.tick_count
        arena.step(10)
        assert arena.tick_count == initial_tick + 10


class TestArenaClone:
    """Test Arena cloning."""

    def test_clone_creates_copy(self, arena_with_car):
        arena, car = arena_with_car
        arena.step(10)

        cloned = arena.clone()

        assert cloned.tick_count == arena.tick_count
        assert cloned.game_mode == arena.game_mode
        assert len(cloned.get_cars()) == len(arena.get_cars())

    def test_clone_is_independent(self, arena_with_car):
        arena, car = arena_with_car
        arena.step(10)

        cloned = arena.clone()
        arena.step(5)

        assert arena.tick_count != cloned.tick_count


class TestArenaBall:
    """Test Arena ball access."""

    def test_ball_exists(self, arena):
        ball = arena.ball
        assert ball is not None

    def test_ball_initial_position(self, arena):
        state = arena.ball.get_state()
        # Ball should start slightly above ground at center
        assert state.pos.x == 0.0
        assert state.pos.y == 0.0
        assert state.pos.z > 0.0  # Above ground


class TestArenaCars:
    """Test Arena car management."""

    def test_add_car(self, arena):
        car = arena.add_car(rs.Team.BLUE, rs.CAR_CONFIG_OCTANE)
        assert car is not None
        # Team is returned as int (0 = BLUE, 1 = ORANGE)
        assert car.team == 0 or car.team == rs.Team.BLUE

    def test_add_multiple_cars(self, arena):
        _car1 = arena.add_car(rs.Team.BLUE, rs.CAR_CONFIG_OCTANE)
        _car2 = arena.add_car(rs.Team.ORANGE, rs.CAR_CONFIG_DOMINUS)

        cars = arena.get_cars()
        assert len(cars) == 2

    def test_get_car_by_id(self, arena):
        car = arena.add_car(rs.Team.BLUE, rs.CAR_CONFIG_OCTANE)
        car_id = car.id

        retrieved = arena.get_car_from_id(car_id)
        assert retrieved.id == car_id

    def test_remove_car(self, arena):
        car = arena.add_car(rs.Team.BLUE, rs.CAR_CONFIG_OCTANE)
        assert len(arena.get_cars()) == 1

        arena.remove_car(car)
        assert len(arena.get_cars()) == 0


class TestArenaBoostPads:
    """Test Arena boost pad access."""

    def test_boost_pads_exist(self, arena):
        pads = arena.get_boost_pads()
        assert len(pads) == 34  # Standard Soccar has 34 boost pads

    def test_boost_pad_properties(self, arena):
        pads = arena.get_boost_pads()
        big_pads = [p for p in pads if p.is_big]
        small_pads = [p for p in pads if not p.is_big]

        assert len(big_pads) == 6  # 6 big boost pads
        assert len(small_pads) == 28  # 28 small boost pads


class TestArenaMutators:
    """Test Arena mutator config."""

    def test_get_mutator_config(self, arena):
        config = arena.get_mutator_config()
        assert config is not None

    def test_set_mutator_config(self, arena):
        config = rs.MutatorConfig()
        config.gravity.z = -500.0  # Moon gravity
        arena.set_mutator_config(config)

        # Verify it was set (get_mutator_config returns reference)
        new_config = arena.get_mutator_config()
        assert new_config.gravity.z == -500.0
