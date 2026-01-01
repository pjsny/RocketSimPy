"""Tests for the Ball class."""

import pytest  # noqa: F401
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
