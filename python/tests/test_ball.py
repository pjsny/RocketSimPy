"""Tests for the Ball class."""

import pytest  # noqa: F401
import pickle
import copy
import RocketSim as rs


class TestBallState:
    """Test Ball state management."""

    def test_get_state(self, arena):
        state = arena.ball.get_state()

        assert isinstance(state.pos, rs.Vec)
        assert isinstance(state.vel, rs.Vec)
        assert isinstance(state.ang_vel, rs.Vec)
        assert isinstance(state.rot_mat, rs.RotMat)

    def test_set_state(self, arena):
        ball = arena.ball
        state = ball.get_state()

        # Modify state
        state.pos.x = 100.0
        state.pos.y = 200.0
        state.pos.z = 300.0
        state.vel.x = 10.0
        state.vel.y = 20.0
        state.vel.z = 30.0

        ball.set_state(state)

        new_state = ball.get_state()
        assert abs(new_state.pos.x - 100.0) < 0.01
        assert abs(new_state.pos.y - 200.0) < 0.01
        assert abs(new_state.pos.z - 300.0) < 0.01
        assert abs(new_state.vel.x - 10.0) < 0.01


class TestBallRotation:
    """Test Ball rotation methods."""

    def test_get_rot_returns_quaternion(self, arena):
        """Test that get_rot returns a quaternion tuple."""
        ball_rot = arena.ball.get_rot()
        assert isinstance(ball_rot, tuple)
        assert len(ball_rot) == 4
        # Quaternion should be normalized (sum of squares ~= 1)
        quat_norm_sq = sum(x * x for x in ball_rot)
        assert abs(quat_norm_sq - 1.0) < 0.001

    def test_get_rot_not_all_zero(self, arena):
        """Test that get_rot doesn't return all zeros."""
        ball_rot = arena.ball.get_rot()
        # At least the w component should be non-zero for identity rotation
        assert sum(abs(x) for x in ball_rot) > 0


class TestBallPhysics:
    """Test Ball physics simulation."""

    def test_ball_falls_with_gravity(self, arena):
        ball = arena.ball
        state = ball.get_state()

        # Set ball in the air with zero velocity
        state.pos.x = 0.0
        state.pos.y = 0.0
        state.pos.z = 500.0
        state.vel.x = 0.0
        state.vel.y = 0.0
        state.vel.z = 0.0
        state.ang_vel.x = 0.0
        state.ang_vel.y = 0.0
        state.ang_vel.z = 0.0
        ball.set_state(state)

        arena.step(60)

        new_state = ball.get_state()
        # Ball should have fallen (or at least not gone up)
        assert new_state.pos.z <= 500.0

    def test_ball_maintains_velocity(self, arena):
        ball = arena.ball
        state = ball.get_state()

        # Set ball moving horizontally
        state.pos.z = 200.0  # Above ground
        state.vel.x = 1000.0
        state.vel.y = 0.0
        state.vel.z = 0.0
        ball.set_state(state)

        arena.step(12)  # 0.1 seconds

        new_state = ball.get_state()
        # Ball should have moved in x direction
        assert new_state.pos.x > 0.0


class TestBallStatePickle:
    """Test BallState pickle serialization."""

    def test_ball_state_pickle_roundtrip(self, arena):
        """Test that BallState can be pickled and unpickled."""
        state = arena.ball.get_state()
        state.pos.x = 1234.5
        state.vel.y = 567.8
        state.ang_vel.z = 1.5

        # Pickle and unpickle
        pickled = pickle.dumps(state)
        restored = pickle.loads(pickled)

        # Check key values
        assert restored.pos.x == state.pos.x
        assert restored.vel.y == state.vel.y
        assert restored.ang_vel.z == state.ang_vel.z

    def test_ball_state_pickle_all_fields(self):
        """Test that all BallState fields survive pickle roundtrip."""
        state = rs.BallState()
        state.pos = rs.Vec(100, 200, 300)
        state.vel = rs.Vec(10, 20, 30)
        state.ang_vel = rs.Vec(1, 2, 3)
        state.last_hit_car_id = 42

        # Pickle and unpickle
        pickled = pickle.dumps(state)
        restored = pickle.loads(pickled)

        # Verify all fields
        assert restored.pos.x == 100
        assert restored.pos.y == 200
        assert restored.pos.z == 300
        assert restored.vel.x == 10
        assert restored.vel.y == 20
        assert restored.vel.z == 30
        assert restored.ang_vel.x == 1
        assert restored.ang_vel.y == 2
        assert restored.ang_vel.z == 3
        assert restored.last_hit_car_id == 42

    def test_ball_state_copy(self):
        """Test that BallState can be copied."""
        state = rs.BallState()
        state.pos = rs.Vec(100, 200, 300)
        state.vel = rs.Vec(10, 20, 30)

        # Shallow copy
        copied = copy.copy(state)

        assert copied.pos.x == 100
        assert copied.vel.y == 20

    def test_ball_state_deepcopy(self):
        """Test that BallState can be deepcopied."""
        state = rs.BallState()
        state.pos = rs.Vec(100, 200, 300)
        state.vel = rs.Vec(10, 20, 30)

        # Deep copy
        copied = copy.deepcopy(state)

        # Modify original
        state.pos.x = 999
        state.vel.y = 888

        # Copy should be unchanged
        assert copied.pos.x == 100
        assert copied.vel.y == 20
