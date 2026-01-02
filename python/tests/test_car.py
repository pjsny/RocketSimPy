"""Tests for the Car class."""

import pytest  # noqa: F401
import pickle
import copy
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
        assert hasattr(state, "has_world_contact")
        assert hasattr(state, "world_contact_normal")


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


class TestCarStateKwargs:
    """Test CarState constructor kwargs."""

    def test_is_flipping_in_constructor(self):
        """Test that is_flipping can be set via constructor."""
        state = rs.CarState(is_flipping=True)
        assert state.is_flipping == True

    def test_has_jumped_in_constructor(self):
        """Test that has_jumped can be set via constructor."""
        state = rs.CarState(has_jumped=True)
        assert state.has_jumped == True

    def test_has_double_jumped_in_constructor(self):
        """Test that has_double_jumped can be set via constructor."""
        state = rs.CarState(has_double_jumped=True)
        assert state.has_double_jumped == True

    def test_has_flipped_in_constructor(self):
        """Test that has_flipped can be set via constructor."""
        state = rs.CarState(has_flipped=True)
        assert state.has_flipped == True

    def test_is_jumping_in_constructor(self):
        """Test that is_jumping can be set via constructor."""
        state = rs.CarState(is_jumping=True)
        assert state.is_jumping == True

    def test_jump_time_in_constructor(self):
        """Test that jump_time can be set via constructor."""
        state = rs.CarState(jump_time=0.5)
        assert abs(state.jump_time - 0.5) < 0.001

    def test_flip_time_in_constructor(self):
        """Test that flip_time can be set via constructor."""
        state = rs.CarState(flip_time=0.3)
        assert abs(state.flip_time - 0.3) < 0.001

    def test_air_time_since_jump_in_constructor(self):
        """Test that air_time_since_jump can be set via constructor."""
        state = rs.CarState(air_time_since_jump=1.0)
        assert abs(state.air_time_since_jump - 1.0) < 0.001

    def test_multiple_kwargs_in_constructor(self):
        """Test setting multiple jump/flip related kwargs."""
        state = rs.CarState(
            pos=rs.Vec(100, 200, 300),
            boost=50.0,
            is_flipping=True,
            has_flipped=True,
            flip_time=0.25,
        )
        assert state.pos.x == 100
        assert state.boost == 50.0
        assert state.is_flipping == True
        assert state.has_flipped == True
        assert abs(state.flip_time - 0.25) < 0.001


class TestCarStatePickle:
    """Test CarState pickle serialization."""

    def test_car_state_pickle_roundtrip(self, arena_with_car):
        """Test that CarState can be pickled and unpickled."""
        arena, car = arena_with_car

        # Get a state with some values
        state = car.get_state()
        state.pos.x = 1234.5
        state.vel.y = 567.8
        state.boost = 75.0
        state.is_flipping = True
        state.has_flipped = True

        # Pickle and unpickle
        pickled = pickle.dumps(state)
        restored = pickle.loads(pickled)

        # Check key values
        assert restored.pos.x == state.pos.x
        assert restored.vel.y == state.vel.y
        assert restored.boost == state.boost
        assert restored.is_flipping == state.is_flipping
        assert restored.has_flipped == state.has_flipped

    def test_car_state_pickle_all_fields(self):
        """Test that all CarState fields survive pickle roundtrip."""
        state = rs.CarState()
        state.pos = rs.Vec(100, 200, 300)
        state.vel = rs.Vec(10, 20, 30)
        state.ang_vel = rs.Vec(1, 2, 3)
        state.boost = 50.0
        state.is_on_ground = True
        state.has_jumped = True
        state.is_jumping = False
        state.has_double_jumped = True
        state.has_flipped = True
        state.is_flipping = True
        state.jump_time = 0.1
        state.flip_time = 0.2
        state.air_time_since_jump = 0.5
        state.is_supersonic = True
        state.is_demoed = False

        # Pickle and unpickle
        pickled = pickle.dumps(state)
        restored = pickle.loads(pickled)

        # Verify all fields
        assert restored.pos.x == 100
        assert restored.vel.y == 20
        assert restored.ang_vel.z == 3
        assert restored.boost == 50.0
        assert restored.is_on_ground == True
        assert restored.has_jumped == True
        assert restored.is_jumping == False
        assert restored.has_double_jumped == True
        assert restored.has_flipped == True
        assert restored.is_flipping == True
        assert abs(restored.jump_time - 0.1) < 0.001
        assert abs(restored.flip_time - 0.2) < 0.001
        assert abs(restored.air_time_since_jump - 0.5) < 0.001
        assert restored.is_supersonic == True
        assert restored.is_demoed == False

    def test_car_state_copy(self):
        """Test that CarState can be copied."""
        state = rs.CarState()
        state.pos = rs.Vec(100, 200, 300)
        state.boost = 75.0
        state.is_flipping = True

        # Shallow copy
        copied = copy.copy(state)

        assert copied.pos.x == 100
        assert copied.boost == 75.0
        assert copied.is_flipping == True

    def test_car_state_deepcopy(self):
        """Test that CarState can be deepcopied."""
        state = rs.CarState()
        state.pos = rs.Vec(100, 200, 300)
        state.boost = 75.0
        state.is_flipping = True

        # Deep copy
        copied = copy.deepcopy(state)

        # Modify original
        state.pos.x = 999
        state.boost = 0.0

        # Copy should be unchanged
        assert copied.pos.x == 100
        assert copied.boost == 75.0
        assert copied.is_flipping == True
