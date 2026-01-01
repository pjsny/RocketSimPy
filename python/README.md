# RocketSim Python Bindings

Python bindings for [RocketSim](https://github.com/ZealanL/RocketSim) built with [nanobind](https://github.com/wjakob/nanobind) for maximum performance.

## Installation

```bash
cd python
uv build --wheel
uv pip install dist/*.whl
```

## RLGym Compatibility

These bindings target full compatibility with [RLGym](https://rlgym.org/). The API matches what rlgym expects from RocketSim >=2.1.

**RLGym Features:**

- Score tracking (`arena.blue_score`, `arena.orange_score`)
- Per-car stats (goals, demos, boost pickups)
- Efficient numpy array getters for gym state
- Goal/bump/demo callbacks
- `get_gym_state()` for one-call state retrieval

## Quick Start

```python
import RocketSim as rs

rs.init("path/to/collision_meshes")

arena = rs.Arena(rs.GameMode.SOCCAR)
car = arena.add_car(rs.Team.BLUE, rs.CAR_CONFIG_OCTANE)

controls = rs.CarControls()
controls.throttle = 1.0
controls.boost = True
car.set_controls(controls)

arena.step(100)

state = car.get_state()
print(f"Position: {state.pos.as_numpy()}")
```

## Efficient State Access (RLGym)

For reinforcement learning, use the efficient numpy array getters:

```python
# Get complete gym state in one call (most efficient)
state = arena.get_gym_state()
# Returns dict with:
#   ball: np.array[18] - [pos(3), vel(3), ang_vel(3), rot_mat(9)]
#   cars: np.array[N, 25] - per-car state
#   pads: np.array[M] - boost pad active states (0/1)
#   blue_score, orange_score, tick_count
#   car_ids, car_teams

# Or get individual components
ball_state = arena.get_ball_state_array()     # shape (18,)
cars_state = arena.get_cars_state_array()     # shape (N, 25)
pads_state = arena.get_pads_state_array()     # shape (M,)

# Car state array layout (25 floats per car):
# [0-2]: pos (x, y, z)
# [3-5]: vel (x, y, z)
# [6-8]: ang_vel (x, y, z)
# [9-17]: rot_mat (forward, right, up flattened)
# [18]: boost
# [19]: is_on_ground
# [20]: has_jumped
# [21]: has_double_jumped
# [22]: has_flipped
# [23]: is_demoed
# [24]: is_supersonic
```

## Callbacks

```python
# Goal scored callback - called with keyword arguments
def on_goal(arena, scoring_team, data):
    print(f"Goal scored by {scoring_team}!")  # Team.BLUE or Team.ORANGE

# Returns previous (callback, data) tuple
prev = arena.set_goal_score_callback(on_goal, my_data)

# Car bump/demo callback - called with keyword arguments
def on_bump(arena, bumper, victim, is_demo, data):
    if is_demo:
        print(f"Car {bumper.id} demoed {victim.id}!")
    else:
        print(f"Car {bumper.id} bumped {victim.id}")

prev = arena.set_car_bump_callback(on_bump, my_data)

# Boost pickup callback - called with keyword arguments
def on_boost_pickup(arena, car, boost_pad, data):
    print(f"Car {car.id} picked up boost!")

prev = arena.set_boost_pickup_callback(on_boost_pickup, my_data)

# Ball touch callback - called with keyword arguments
def on_ball_touch(arena, car, data):
    print(f"Car {car.id} touched the ball!")

prev = arena.set_ball_touch_callback(on_ball_touch, my_data)
```

**Note:** Goal and boost pickup callbacks are not available in `THE_VOID` game mode.
Ball touch callback is only registered when set (to avoid overhead).

## API Reference

### Core

```python
rs.init(path)                    # Initialize with collision meshes
rs.Arena(game_mode, tick_rate)   # Create simulation arena
# tick_rate: 15-120 (default: 120)

rs.GameMode.SOCCAR / .HOOPS / .HEATSEEKER / .SNOWDAY / .DROPSHOT / .THE_VOID
rs.Team.BLUE / .ORANGE
```

### Arena

```python
arena.step(ticks)                # Advance simulation
arena.clone(copy_callbacks=False)  # Deep copy (preserves scores/stats)
arena.add_car(team, config)      # Add car, returns Car
arena.remove_car(car)
arena.get_cars()                 # List of cars
arena.get_car_from_id(id, default=None)  # Get car by ID or default
arena.get_boost_pads()           # List of boost pads
arena.ball                       # Ball object
arena.tick_count, .tick_rate, .tick_time
arena.reset_to_random_kickoff(seed=-1)
arena.is_ball_scored()           # Check if ball is in goal

# Score tracking
arena.blue_score, arena.orange_score

# Per-car stats
arena.get_car_goals(car_id)
arena.get_car_demos(car_id)
arena.get_car_boost_pickups(car_id)

# Efficient gym state
arena.get_gym_state()            # Complete state as dict
arena.get_ball_state_array()     # Ball state as np.array(18,)
arena.get_car_state_array(car)   # Single car as np.array(25,)
arena.get_cars_state_array()     # All cars as np.array(N, 25)
arena.get_pads_state_array()     # Boost pads as np.array(M,)
```

### Car

```python
car.id, car.team
car.get_state() / .set_state(state)
car.get_controls() / .set_controls(controls)
car.demolish(respawn_delay=3.0)
car.respawn(game_mode, seed=-1, boost_amount=33.33)
```

### CarState

```python
state.pos, .vel, .ang_vel        # Vec
state.rot_mat                    # RotMat
state.boost                      # float [0, 100]
state.is_on_ground, .is_supersonic, .is_demoed
state.has_jumped, .has_double_jumped, .has_flipped
state.has_world_contact          # bool
state.world_contact_normal       # Vec
```

### CarControls

```python
controls.throttle, .steer        # float [-1, 1]
controls.pitch, .yaw, .roll      # float [-1, 1]
controls.boost, .jump, .handbrake  # bool
```

### Vec / RotMat / Angle

```python
vec = rs.Vec(x, y, z)
vec.as_numpy()                   # np.array([x, y, z])
vec == other                     # Rich comparison support
vec < other                      # Tuple-style ordering
hash(vec)                        # Hashable (can use in sets/dicts)

rot = rs.RotMat()
rot = rs.RotMat(forward=rs.Vec(1, 0, 0))  # kwargs constructor
rot.forward, .right, .up         # Vec
rot.as_numpy()                   # np.array((3, 3))

angle = rs.Angle(yaw=1.0, pitch=0.5)  # kwargs constructor
angle.yaw, .pitch, .roll         # float
```

### Classes with kwargs constructors

All state classes support kwargs for convenient initialization:

```python
# CarState with kwargs
state = rs.CarState(pos=rs.Vec(0, 0, 17), boost=100, is_on_ground=True)

# BallState with kwargs
ball_state = rs.BallState(pos=rs.Vec(0, 0, 100), vel=rs.Vec(1000, 0, 0))

# CarControls with kwargs
controls = rs.CarControls(throttle=1.0, boost=True, jump=False)

# Angle with kwargs
angle = rs.Angle(yaw=1.57)

# RotMat with kwargs
rot = rs.RotMat(forward=rs.Vec(1, 0, 0))
```

### Car Configs

```python
# Create car config from hitbox type (preferred)
config = rs.CarConfig(rs.CarConfig.OCTANE)  # or rs.CarConfig.DOMINUS, etc.

# Hitbox type constants (also available as CarConfig class attributes)
rs.CarConfig.OCTANE    # = 0
rs.CarConfig.DOMINUS   # = 1
rs.CarConfig.PLANK     # = 2
rs.CarConfig.BREAKOUT  # = 3
rs.CarConfig.HYBRID    # = 4
rs.CarConfig.MERC      # = 5

# Module-level constants (alternative)
rs.OCTANE, rs.DOMINUS, rs.PLANK, rs.BREAKOUT, rs.HYBRID, rs.MERC

# Full config references (read-only)
rs.CAR_CONFIG_OCTANE, rs.CAR_CONFIG_DOMINUS, ...
```

## Credits

- [ZealanL/RocketSim](https://github.com/ZealanL/RocketSim) — Original C++ implementation
- [mtheall/RocketSim](https://github.com/mtheall/RocketSim) — Python bindings inspiration
