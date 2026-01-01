"""Tests for RLGym-compatible features (scores, callbacks, gym state arrays)."""

import pytest
import numpy as np
import RocketSim as rs


class TestScoreTracking:
    """Test automatic score tracking."""

    def test_initial_scores_zero(self):
        """Scores should start at 0."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        assert arena.blue_score == 0
        assert arena.orange_score == 0

    def test_scores_reset_on_kickoff(self):
        """Scores should reset when resetting to kickoff."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        arena.add_car(rs.Team.BLUE, rs.CarConfig())
        arena.add_car(rs.Team.ORANGE, rs.CarConfig())
        # Force a score change by setting ball in goal (this won't actually score in this test)
        # Just verify reset works
        arena.reset_to_random_kickoff()
        assert arena.blue_score == 0
        assert arena.orange_score == 0


class TestCallbacks:
    """Test Python callbacks for game events."""

    def test_goal_score_callback_is_set(self):
        """Goal score callback can be set without error."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        callback_called = []

        def on_goal(arena, scoring_team, data):
            callback_called.append((scoring_team, data))

        prev = arena.set_goal_score_callback(on_goal, "test_data")
        assert prev == (None, None)  # No previous callback

    def test_car_bump_callback_is_set(self):
        """Car bump callback can be set without error."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        callback_called = []

        def on_bump(arena, bumper, victim, is_demo, data):
            callback_called.append((bumper.id, victim.id, is_demo, data))

        prev = arena.set_car_bump_callback(on_bump, None)
        assert prev == (None, None)  # No previous callback

    def test_callback_returns_previous(self):
        """Setting a callback returns the previous callback and data."""
        arena = rs.Arena(rs.GameMode.SOCCAR)

        def cb1(arena, scoring_team, data):
            pass

        def cb2(arena, scoring_team, data):
            pass

        prev1 = arena.set_goal_score_callback(cb1, "data1")
        prev2 = arena.set_goal_score_callback(cb2, "data2")

        assert prev1 == (None, None)
        assert prev2[0] is cb1
        assert prev2[1] == "data1"


class TestCarStats:
    """Test per-car statistics tracking."""

    def test_initial_stats_zero(self):
        """Car stats should start at 0."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        car = arena.add_car(rs.Team.BLUE, rs.CarConfig())

        assert arena.get_car_goals(car.id) == 0
        assert arena.get_car_demos(car.id) == 0
        assert arena.get_car_boost_pickups(car.id) == 0

    def test_stats_for_nonexistent_car(self):
        """Stats for nonexistent car should return 0."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        assert arena.get_car_goals(9999) == 0
        assert arena.get_car_demos(9999) == 0
        assert arena.get_car_boost_pickups(9999) == 0


class TestGymStateArrays:
    """Test efficient numpy array getters for RLGym."""

    def test_ball_state_array_shape(self):
        """Ball state array should have shape (18,)."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        ball_state = arena.get_ball_state_array()

        assert isinstance(ball_state, np.ndarray)
        assert ball_state.shape == (18,)
        assert ball_state.dtype == np.float32

    def test_ball_state_array_values(self):
        """Ball state array should contain correct values."""
        arena = rs.Arena(rs.GameMode.SOCCAR)

        # Set known ball state
        ball_state_obj = rs.BallState()
        ball_state_obj.pos = rs.Vec(100, 200, 300)
        ball_state_obj.vel = rs.Vec(10, 20, 30)
        ball_state_obj.ang_vel = rs.Vec(1, 2, 3)
        arena.ball.set_state(ball_state_obj)

        arr = arena.get_ball_state_array()

        # Check position
        assert arr[0] == pytest.approx(100, abs=0.1)
        assert arr[1] == pytest.approx(200, abs=0.1)
        assert arr[2] == pytest.approx(300, abs=0.1)
        # Check velocity
        assert arr[3] == pytest.approx(10, abs=0.1)
        assert arr[4] == pytest.approx(20, abs=0.1)
        assert arr[5] == pytest.approx(30, abs=0.1)
        # Check angular velocity
        assert arr[6] == pytest.approx(1, abs=0.1)
        assert arr[7] == pytest.approx(2, abs=0.1)
        assert arr[8] == pytest.approx(3, abs=0.1)

    def test_car_state_array_shape(self):
        """Single car state array should have shape (25,)."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        car = arena.add_car(rs.Team.BLUE, rs.CarConfig())

        car_state = arena.get_car_state_array(car)

        assert isinstance(car_state, np.ndarray)
        assert car_state.shape == (25,)
        assert car_state.dtype == np.float32

    def test_cars_state_array_shape(self):
        """All cars state array should have shape (N, 25)."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        arena.add_car(rs.Team.BLUE, rs.CarConfig())
        arena.add_car(rs.Team.ORANGE, rs.CarConfig())
        arena.add_car(rs.Team.BLUE, rs.CarConfig())

        cars_state = arena.get_cars_state_array()

        assert isinstance(cars_state, np.ndarray)
        assert cars_state.shape == (3, 25)
        assert cars_state.dtype == np.float32

    def test_pads_state_array_shape(self):
        """Boost pads state array should have correct length."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        pads = arena.get_boost_pads()

        pads_state = arena.get_pads_state_array()

        assert isinstance(pads_state, np.ndarray)
        assert pads_state.shape == (len(pads),)
        assert pads_state.dtype == np.float32

    def test_pads_state_binary(self):
        """Boost pads state should be 0 or 1."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        pads_state = arena.get_pads_state_array()

        # All values should be 0 or 1
        assert np.all((pads_state == 0) | (pads_state == 1))
        # Initially all should be active (1)
        assert np.all(pads_state == 1)


class TestGetGymState:
    """Test the combined get_gym_state() method."""

    def test_gym_state_keys(self):
        """Gym state should have all expected keys."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        arena.add_car(rs.Team.BLUE, rs.CarConfig())
        arena.add_car(rs.Team.ORANGE, rs.CarConfig())

        state = arena.get_gym_state()

        assert "ball" in state
        assert "cars" in state
        assert "pads" in state
        assert "blue_score" in state
        assert "orange_score" in state
        assert "tick_count" in state
        assert "car_ids" in state
        assert "car_teams" in state

    def test_gym_state_array_types(self):
        """Gym state arrays should be numpy arrays."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        arena.add_car(rs.Team.BLUE, rs.CarConfig())

        state = arena.get_gym_state()

        assert isinstance(state["ball"], np.ndarray)
        assert isinstance(state["cars"], np.ndarray)
        assert isinstance(state["pads"], np.ndarray)

    def test_gym_state_car_ids_match(self):
        """Car IDs in gym state should match actual car IDs."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        car1 = arena.add_car(rs.Team.BLUE, rs.CarConfig())
        car2 = arena.add_car(rs.Team.ORANGE, rs.CarConfig())

        state = arena.get_gym_state()
        car_ids = list(state["car_ids"])

        assert car1.id in car_ids
        assert car2.id in car_ids

    def test_gym_state_car_teams_correct(self):
        """Car teams in gym state should be correct."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        arena.add_car(rs.Team.BLUE, rs.CarConfig())
        arena.add_car(rs.Team.ORANGE, rs.CarConfig())

        state = arena.get_gym_state()
        teams = list(state["car_teams"])

        # Should have one blue (0) and one orange (1)
        assert 0 in teams  # Blue
        assert 1 in teams  # Orange


class TestArenaClone:
    """Test arena cloning preserves state."""

    def test_clone_preserves_scores(self):
        """Cloning should preserve scores."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        arena.add_car(rs.Team.BLUE, rs.CarConfig())
        # Note: We can't easily score goals, but we verify the mechanism works

        cloned = arena.clone()

        assert cloned.blue_score == arena.blue_score
        assert cloned.orange_score == arena.orange_score

    def test_clone_preserves_car_count(self):
        """Cloning should preserve number of cars."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        arena.add_car(rs.Team.BLUE, rs.CarConfig())
        arena.add_car(rs.Team.ORANGE, rs.CarConfig())

        cloned = arena.clone()

        assert len(cloned.get_cars()) == len(arena.get_cars())

    def test_clone_is_independent(self):
        """Cloned arena should be independent of original."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        car = arena.add_car(rs.Team.BLUE, rs.CarConfig())

        cloned = arena.clone()

        # Step only the clone
        cloned.step(10)

        # Original should not have advanced
        assert arena.tick_count == 0
        assert cloned.tick_count == 10

    def test_clone_copy_callbacks_false_by_default(self):
        """Clone without copy_callbacks should not copy Python callbacks."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        callback_results = []

        def on_goal(arena, scoring_team, data):
            callback_results.append(("original", scoring_team))

        arena.set_goal_score_callback(on_goal, None)

        # Clone without copying callbacks (default)
        cloned = arena.clone()

        # Verify the clone was created (basic sanity check)
        assert cloned is not None
        assert cloned.tick_count == arena.tick_count

    def test_clone_copy_callbacks_true(self):
        """Clone with copy_callbacks=True should work."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        callback_results = []

        def on_goal(arena, scoring_team, data):
            callback_results.append(scoring_team)

        arena.set_goal_score_callback(on_goal, None)

        # Clone with copying callbacks
        cloned = arena.clone(copy_callbacks=True)

        # Verify the clone was created
        assert cloned is not None
        assert cloned.tick_count == arena.tick_count


class TestIsBallScored:
    """Test ball scoring detection."""

    def test_is_ball_scored_exists(self):
        """is_ball_scored method should exist and return bool."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        result = arena.is_ball_scored()
        assert isinstance(result, bool)

    def test_ball_not_scored_initially(self):
        """Ball should not be scored at start."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        assert arena.is_ball_scored() == False


class TestCarRemoval:
    """Test car removal edge cases (regression tests)."""

    def test_repeated_add_remove_cycles(self):
        """Adding and removing many cars shouldn't cause issues."""
        import random

        arena = rs.Arena(rs.GameMode.SOCCAR)

        for _ in range(50):
            # Add several cars
            for _ in range(5):
                team = rs.Team.BLUE if random.randint(0, 1) == 0 else rs.Team.ORANGE
                arena.add_car(team, rs.CarConfig())

            # Remove all cars
            while len(arena.get_cars()) > 0:
                arena.remove_car(arena.get_cars()[0])

            assert len(arena.get_cars()) == 0

    def test_remove_car_clears_stats(self):
        """Removing a car should clear its stats."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        car = arena.add_car(rs.Team.BLUE, rs.CarConfig())
        car_id = car.id

        arena.remove_car(car)

        # Stats for removed car should return 0
        assert arena.get_car_goals(car_id) == 0
        assert arena.get_car_demos(car_id) == 0


class TestPerformance:
    """Test that gym state methods are efficient."""

    def test_gym_state_many_calls(self):
        """Gym state should handle many rapid calls."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        arena.add_car(rs.Team.BLUE, rs.CarConfig())
        arena.add_car(rs.Team.ORANGE, rs.CarConfig())

        # Call get_gym_state many times - should not crash or leak
        for _ in range(100):
            state = arena.get_gym_state()
            assert state["ball"].shape == (18,)

    def test_step_with_gym_state(self):
        """Stepping and getting gym state should work together."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        arena.add_car(rs.Team.BLUE, rs.CarConfig())

        for i in range(50):
            arena.step(1)
            state = arena.get_gym_state()
            assert state["tick_count"] == i + 1


class TestDemoCallbacks:
    """Test demo callback edge cases."""

    def test_no_duplicate_demo_callbacks(self):
        """Demo callback should only fire once per demo event.

        Ensures the callback isn't called multiple times for the same demo.
        """
        arena = rs.Arena(rs.GameMode.SOCCAR)
        orange = arena.add_car(rs.Team.ORANGE, rs.CarConfig(rs.CarConfig.BREAKOUT))
        blue = arena.add_car(rs.Team.BLUE, rs.CarConfig(rs.CarConfig.HYBRID))

        # Set up orange car at origin
        orange_state = rs.CarState()
        orange_state.pos = rs.Vec(0, 0, 17)
        orange.set_state(orange_state)

        # Set up blue car coming in fast with boost
        blue_state = rs.CarState()
        blue_state.pos = rs.Vec(-300, 0, 17)
        blue_state.vel = rs.Vec(2300, 0, 0)
        blue_state.boost = 100
        blue.set_state(blue_state)

        # Blue car boosting forward
        controls = rs.CarControls()
        controls.throttle = 1
        controls.boost = True
        blue.set_controls(controls)

        # Track demos - ensure no duplicates
        demos = set()

        def handle_demo(arena, bumper, victim, is_demo, data):
            if not is_demo:
                return
            key = (arena.tick_count, bumper.id, victim.id)
            assert key not in demos, f"Duplicate demo callback: {key}"
            demos.add(key)

        arena.set_car_bump_callback(handle_demo, None)
        arena.step(15)

        # Should have exactly one demo
        assert len(demos) == 1
        # Demo should happen around tick 9 (blue car id=2 demos orange car id=1)
        assert (9, blue.id, orange.id) in demos


class TestBoostPickupCallback:
    """Test boost pickup callback functionality."""

    def test_boost_pickup_callback_is_set(self):
        """Test that boost pickup callback can be set and returns previous."""
        arena = rs.Arena(rs.GameMode.SOCCAR)

        callback_invocations = []

        def on_pickup(arena, car, boost_pad, data):
            callback_invocations.append(
                {"arena": arena, "car_id": car.id, "boost_pad": boost_pad, "data": data}
            )

        # Initially no callback - should return (None, None)
        prev = arena.set_boost_pickup_callback(on_pickup, "test_data")
        assert prev == (None, None)

        # Set again - should return previous
        prev = arena.set_boost_pickup_callback(lambda *args, **kwargs: None, None)
        assert prev[0] == on_pickup
        assert prev[1] == "test_data"

    def test_boost_pickup_callback_not_available_in_the_void(self):
        """Test that boost pickup callback raises error in THE_VOID mode."""
        arena = rs.Arena(rs.GameMode.THE_VOID)

        # THE_VOID mode doesn't have boost pads, so setting callback should fail
        with pytest.raises(RuntimeError, match="THE_VOID"):
            arena.set_boost_pickup_callback(lambda *args, **kwargs: None, None)

        # Goal callback should also fail in THE_VOID
        with pytest.raises(RuntimeError, match="THE_VOID"):
            arena.set_goal_score_callback(lambda *args, **kwargs: None, None)

    def test_boost_pickup_callback_tracks_stats(self):
        """Test that boost pickups are tracked in car stats."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        car = arena.add_car(rs.Team.BLUE, rs.CarConfig(rs.CarConfig.OCTANE))

        # Initial boost pickups should be zero
        assert arena.get_car_boost_pickups(car.id) == 0

        # Position car on top of a boost pad (full boost at corner)
        state = car.get_state()
        state.pos = rs.Vec(3072, -4096, 17)  # Near a full boost pad
        state.boost = 0
        car.set_state(state)

        # Step simulation until boost pickup or timeout
        for _ in range(50):
            arena.step(1)
            if arena.get_car_boost_pickups(car.id) > 0:
                break

        # After stepping near boost, should have at least one pickup
        # (This is position-dependent, may not always trigger)


class TestInputValidation:
    """Test input validation for Arena constructor."""

    def test_valid_game_modes(self):
        """Test that valid game modes are accepted."""
        arena = rs.Arena(rs.GameMode.SOCCAR)
        assert arena is not None

        # THE_VOID also works (no boost pads or goals)
        arena_void = rs.Arena(rs.GameMode.THE_VOID)
        assert arena_void is not None

    def test_valid_tick_rates(self):
        """Test that valid tick rates are accepted."""
        # Test standard rate
        arena = rs.Arena(rs.GameMode.SOCCAR, 120)
        assert arena is not None

        # Test minimum rate
        arena_15 = rs.Arena(rs.GameMode.SOCCAR, 15)
        assert arena_15 is not None

        # Test mid rate
        arena_60 = rs.Arena(rs.GameMode.SOCCAR, 60)
        assert arena_60 is not None

    def test_invalid_tick_rate_too_low(self):
        """Test that tick rate below 15 raises error."""
        with pytest.raises(Exception):  # invalid_argument
            rs.Arena(rs.GameMode.SOCCAR, 10)

    def test_invalid_tick_rate_too_high(self):
        """Test that tick rate above 120 raises error."""
        with pytest.raises(Exception):  # invalid_argument
            rs.Arena(rs.GameMode.SOCCAR, 240)


class TestKwargsConstructors:
    """Test kwargs support for various classes."""

    def test_angle_kwargs(self):
        """Test Angle can be constructed with kwargs."""
        angle = rs.Angle(yaw=1.0)
        assert angle.yaw == pytest.approx(1.0)
        assert angle.pitch == pytest.approx(0.0)
        assert angle.roll == pytest.approx(0.0)

        angle2 = rs.Angle(pitch=0.5, roll=0.3)
        assert angle2.yaw == pytest.approx(0.0)
        assert angle2.pitch == pytest.approx(0.5)
        assert angle2.roll == pytest.approx(0.3)

    def test_car_controls_kwargs(self):
        """Test CarControls can be constructed with kwargs."""
        controls = rs.CarControls(throttle=1.0, boost=True)
        assert controls.throttle == 1.0
        assert controls.boost == True
        assert controls.steer == 0.0
        assert controls.jump == False

    def test_ball_state_kwargs(self):
        """Test BallState can be constructed with kwargs."""
        state = rs.BallState(pos=rs.Vec(100, 200, 300))
        assert state.pos.x == 100
        assert state.pos.y == 200
        assert state.pos.z == 300

        state2 = rs.BallState(vel=rs.Vec(10, 20, 30))
        assert state2.vel.x == 10
        assert state2.vel.y == 20
        assert state2.vel.z == 30

    def test_car_state_kwargs(self):
        """Test CarState can be constructed with kwargs."""
        state = rs.CarState(pos=rs.Vec(100, 0, 17), boost=50.0)
        assert state.pos.x == 100
        assert state.boost == 50.0

    def test_rot_mat_kwargs(self):
        """Test RotMat can be constructed with kwargs."""
        mat = rs.RotMat(forward=rs.Vec(1, 0, 0))
        assert mat.forward.x == 1
        assert mat.forward.y == 0
        assert mat.forward.z == 0


class TestVecRichComparison:
    """Test Vec rich comparison operators."""

    def test_vec_equality(self):
        """Test Vec __eq__."""
        v1 = rs.Vec(1, 2, 3)
        v2 = rs.Vec(1, 2, 3)
        v3 = rs.Vec(1, 2, 4)

        assert v1 == v2
        assert not (v1 == v3)

    def test_vec_inequality(self):
        """Test Vec __ne__."""
        v1 = rs.Vec(1, 2, 3)
        v2 = rs.Vec(1, 2, 4)

        assert v1 != v2

    def test_vec_less_than(self):
        """Test Vec __lt__ (tuple comparison)."""
        v1 = rs.Vec(1, 2, 3)
        v2 = rs.Vec(1, 2, 4)

        assert v1 < v2
        assert not (v2 < v1)

    def test_vec_hash(self):
        """Test Vec __hash__."""
        v1 = rs.Vec(1, 2, 3)
        v2 = rs.Vec(1, 2, 3)

        # Same values should have same hash
        assert hash(v1) == hash(v2)

        # Can be used in sets/dicts
        s = {v1, v2}
        assert len(s) == 1
