"""Basic tests for RocketSim Python bindings."""

import pytest
import numpy as np
import RocketSim as rs


class TestImport:
    """Test that the module imports correctly."""

    def test_module_has_expected_attributes(self):
        """Check that all expected classes and functions exist."""
        expected = [
            "init",
            "Arena",
            "Ball",
            "Car",
            "BoostPad",
            "Vec",
            "RotMat",
            "Angle",
            "BallState",
            "CarState",
            "CarControls",
            "CarConfig",
            "BoostPadState",
            "MutatorConfig",
            "GameMode",
            "Team",
            "DemoMode",
            "CAR_CONFIG_OCTANE",
            "CAR_CONFIG_DOMINUS",
            "CAR_CONFIG_PLANK",
            "CAR_CONFIG_BREAKOUT",
            "CAR_CONFIG_HYBRID",
            "CAR_CONFIG_MERC",
        ]
        for name in expected:
            assert hasattr(rs, name), f"Missing attribute: {name}"


class TestVec:
    """Test the Vec class."""

    def test_default_constructor(self):
        v = rs.Vec()
        assert v.x == 0.0
        assert v.y == 0.0
        assert v.z == 0.0

    def test_constructor_with_args(self):
        v = rs.Vec(1.0, 2.0, 3.0)
        assert v.x == 1.0
        assert v.y == 2.0
        assert v.z == 3.0

    def test_as_numpy(self):
        v = rs.Vec(1.0, 2.0, 3.0)
        arr = v.as_numpy()
        assert isinstance(arr, np.ndarray)
        assert arr.shape == (3,)
        np.testing.assert_array_almost_equal(arr, [1.0, 2.0, 3.0])


class TestRotMat:
    """Test the RotMat class."""

    def test_default_constructor(self):
        r = rs.RotMat()
        assert isinstance(r.forward, rs.Vec)
        assert isinstance(r.right, rs.Vec)
        assert isinstance(r.up, rs.Vec)

    def test_as_numpy(self):
        r = rs.RotMat()
        arr = r.as_numpy()
        assert isinstance(arr, np.ndarray)
        assert arr.shape == (3, 3)


class TestAngle:
    """Test the Angle class."""

    def test_default_constructor(self):
        a = rs.Angle()
        assert a.yaw == 0.0
        assert a.pitch == 0.0
        assert a.roll == 0.0

    def test_constructor_with_args(self):
        a = rs.Angle(1.0, 2.0, 3.0)
        assert a.yaw == 1.0
        assert a.pitch == 2.0
        assert a.roll == 3.0


class TestGameMode:
    """Test GameMode enum."""

    def test_game_modes_exist(self):
        assert rs.GameMode.SOCCAR is not None
        assert rs.GameMode.HOOPS is not None
        assert rs.GameMode.HEATSEEKER is not None
        assert rs.GameMode.SNOWDAY is not None
        assert rs.GameMode.DROPSHOT is not None
        assert rs.GameMode.THE_VOID is not None


class TestTeam:
    """Test Team enum."""

    def test_teams_exist(self):
        assert rs.Team.BLUE is not None
        assert rs.Team.ORANGE is not None


class TestDemoMode:
    """Test DemoMode enum."""

    def test_demo_modes_exist(self):
        assert rs.DemoMode.NORMAL is not None
        assert rs.DemoMode.ON_CONTACT is not None
        assert rs.DemoMode.DISABLED is not None


class TestCarConfig:
    """Test CarConfig with different hitbox types."""

    def test_default_config_is_empty(self):
        """CarConfig() creates an empty config - use CarConfig(rs.OCTANE) for presets."""
        config = rs.CarConfig()
        assert config.hitbox_size.x == 0.0  # Empty config

    def test_octane_config(self):
        config = rs.CarConfig(rs.OCTANE)
        assert config.hitbox_size.x == pytest.approx(120.507, abs=0.01)

    def test_dominus_config(self):
        config = rs.CarConfig(rs.DOMINUS)
        assert config.hitbox_size.x == pytest.approx(130.427, abs=0.01)

    def test_plank_config(self):
        config = rs.CarConfig(rs.PLANK)
        assert config.hitbox_size.x == pytest.approx(131.32, abs=0.01)

    def test_breakout_config(self):
        config = rs.CarConfig(rs.BREAKOUT)
        assert config.hitbox_size.x == pytest.approx(133.992, abs=0.01)

    def test_hybrid_config(self):
        config = rs.CarConfig(rs.HYBRID)
        assert config.hitbox_size.x == pytest.approx(129.519, abs=0.01)

    def test_merc_config(self):
        config = rs.CarConfig(rs.MERC)
        assert config.hitbox_size.x == pytest.approx(123.22, abs=0.01)

    def test_dodge_deadzone_default(self):
        config = rs.CarConfig()
        assert config.dodge_deadzone == pytest.approx(0.5, abs=0.01)

    def test_class_constants_exist(self):
        """CarConfig should have class-level constants for hitbox types."""
        assert rs.CarConfig.OCTANE == 0
        assert rs.CarConfig.DOMINUS == 1
        assert rs.CarConfig.PLANK == 2
        assert rs.CarConfig.BREAKOUT == 3
        assert rs.CarConfig.HYBRID == 4
        assert rs.CarConfig.MERC == 5

    def test_class_constant_with_constructor(self):
        """CarConfig(CarConfig.DOMINUS) should work like CarConfig(rs.DOMINUS)."""
        config = rs.CarConfig(rs.CarConfig.DOMINUS)
        assert config.hitbox_size.x == pytest.approx(130.427, abs=0.01)
