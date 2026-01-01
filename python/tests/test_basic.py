"""Basic tests for RocketSim Python bindings."""
import pytest
import numpy as np
import RocketSim as rs


class TestImport:
    """Test that the module imports correctly."""
    
    def test_module_has_expected_attributes(self):
        """Check that all expected classes and functions exist."""
        expected = [
            'init', 'Arena', 'Ball', 'Car', 'BoostPad',
            'Vec', 'RotMat', 'Angle',
            'BallState', 'CarState', 'CarControls', 'CarConfig',
            'BoostPadState', 'MutatorConfig',
            'GameMode', 'Team', 'DemoMode',
            'CAR_CONFIG_OCTANE', 'CAR_CONFIG_DOMINUS',
            'CAR_CONFIG_PLANK', 'CAR_CONFIG_BREAKOUT',
            'CAR_CONFIG_HYBRID', 'CAR_CONFIG_MERC',
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

