import pytest
import os
import RocketSim as rs


@pytest.fixture(scope="session", autouse=True)
def init_rocketsim():
    """Initialize RocketSim once for all tests."""
    # Find collision_meshes directory relative to this file
    tests_dir = os.path.dirname(__file__)
    python_dir = os.path.dirname(tests_dir)
    repo_root = os.path.dirname(python_dir)
    collision_meshes = os.path.join(repo_root, "collision_meshes")
    
    rs.init(collision_meshes)
    yield


@pytest.fixture
def arena():
    """Create a fresh arena for each test."""
    return rs.Arena(rs.GameMode.SOCCAR)


@pytest.fixture
def arena_with_car(arena):
    """Create an arena with one car."""
    car = arena.add_car(rs.Team.BLUE, rs.CAR_CONFIG_OCTANE)
    return arena, car

