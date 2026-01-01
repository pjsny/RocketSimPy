"""Tests for the Car class."""

import pytest  # noqa: F401
import RocketSim as rs


class TestCarProperties:
    """Test Car properties."""

    def test_car_id(self, arena_with_car):
        arena, car = arena_with_car
        assert isinstance(car.id, int)
        assert car.id > 0

    def test_car_team(self, arena_with_car):
        arena, car = arena_with_car
        # Team is returned as int (0 = BLUE, 1 = ORANGE)
        assert car.team == 0 or car.team == rs.Team.BLUE


class TestCarState:
    """Test Car state management."""

    def test_get_state(self, arena_with_car):
        arena, car = arena_with_car
        state = car.get_state()

        assert isinstance(state.pos, rs.Vec)
        assert isinstance(state.vel, rs.Vec)
        assert isinstance(state.ang_vel, rs.Vec)
        assert isinstance(state.rot_mat, rs.RotMat)

    def test_set_state(self, arena_with_car):
        arena, car = arena_with_car
        state = car.get_state()

        # Modify position
        state.pos.x = 1000.0
        state.pos.y = 500.0
        state.pos.z = 100.0
        state.boost = 100.0

        car.set_state(state)

        new_state = car.get_state()
        assert new_state.pos.x == 1000.0
        assert new_state.pos.y == 500.0
        assert new_state.boost == 100.0

    def test_car_state_attributes(self, arena_with_car):
        arena, car = arena_with_car
        state = car.get_state()

        # Check all expected attributes exist
        assert hasattr(state, "pos")
        assert hasattr(state, "vel")
        assert hasattr(state, "ang_vel")
        assert hasattr(state, "rot_mat")
        assert hasattr(state, "boost")
        assert hasattr(state, "is_on_ground")
        assert hasattr(state, "has_jumped")
        assert hasattr(state, "has_double_jumped")
        assert hasattr(state, "has_flipped")
        assert hasattr(state, "is_supersonic")


class TestCarControls:
    """Test Car controls."""

    def test_set_controls(self, arena_with_car):
        arena, car = arena_with_car

        controls = rs.CarControls()
        controls.throttle = 1.0
        controls.steer = 0.5
        controls.boost = True

        car.set_controls(controls)
        # Note: Controls are applied on next step
        arena.step(1)

    def test_controls_default_values(self):
        controls = rs.CarControls()

        assert controls.throttle == 0.0
        assert controls.steer == 0.0
        assert controls.pitch == 0.0
        assert controls.yaw == 0.0
        assert controls.roll == 0.0
        assert not controls.boost
        assert not controls.jump
        assert not controls.handbrake


class TestCarSimulation:
    """Test car physics simulation."""

    def test_car_falls_with_gravity(self, arena_with_car):
        arena, car = arena_with_car

        # Set car in the air
        state = car.get_state()
        state.pos.z = 500.0
        state.vel.z = 0.0
        car.set_state(state)

        arena.step(60)  # 0.5 seconds at 120 tick rate

        new_state = car.get_state()
        # Car should have fallen
        assert new_state.pos.z < 500.0

    def test_car_moves_with_throttle(self, arena_with_car):
        arena, car = arena_with_car

        initial_state = car.get_state()
        initial_y = initial_state.pos.y

        # Apply throttle
        controls = rs.CarControls()
        controls.throttle = 1.0
        car.set_controls(controls)

        # Step simulation
        arena.step(60)

        # Car should have moved forward (positive Y in Rocket League)
        new_state = car.get_state()
        assert new_state.pos.y > initial_y

    def test_car_boost_consumption(self, arena_with_car):
        arena, car = arena_with_car

        # Set boost to 100
        state = car.get_state()
        state.boost = 100.0
        car.set_state(state)

        # Apply boost
        controls = rs.CarControls()
        controls.throttle = 1.0
        controls.boost = True
        car.set_controls(controls)

        arena.step(60)

        # Boost should have been consumed
        new_state = car.get_state()
        assert new_state.boost < 100.0
