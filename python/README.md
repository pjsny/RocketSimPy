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

## API Reference

### Core

```python
rs.init(path)                    # Initialize with collision meshes
rs.Arena(game_mode, tick_rate)   # Create simulation arena
rs.GameMode.SOCCAR / .HOOPS / .HEATSEEKER / .SNOWDAY / .DROPSHOT
rs.Team.BLUE / .ORANGE
```

### Arena

```python
arena.step(ticks)                # Advance simulation
arena.clone()                    # Deep copy
arena.add_car(team, config)      # Add car, returns Car
arena.remove_car(car)
arena.get_cars()                 # List of cars
arena.get_boost_pads()           # List of boost pads
arena.ball                       # Ball object
arena.tick_count, .tick_rate
```

### Car

```python
car.id, car.team
car.get_state() / .set_state(state)
car.get_controls() / .set_controls(controls)
```

### CarState

```python
state.pos, .vel, .ang_vel        # Vec
state.rot_mat                    # RotMat
state.boost                      # float [0, 100]
state.is_on_ground, .is_supersonic, .is_demoed
state.has_jumped, .has_double_jumped, .has_flipped
```

### CarControls

```python
controls.throttle, .steer        # float [-1, 1]
controls.pitch, .yaw, .roll      # float [-1, 1]
controls.boost, .jump, .handbrake  # bool
```

### Vec / RotMat

```python
vec = rs.Vec(x, y, z)
vec.as_numpy()                   # np.array([x, y, z])

rot = rs.RotMat()
rot.forward, .right, .up         # Vec
rot.as_numpy()                   # np.array((3, 3))
```

### Car Configs

```python
rs.CAR_CONFIG_OCTANE, rs.CAR_CONFIG_DOMINUS
rs.CAR_CONFIG_PLANK, rs.CAR_CONFIG_BREAKOUT
rs.CAR_CONFIG_HYBRID, rs.CAR_CONFIG_MERC
```

## Credits

- [ZealanL/RocketSim](https://github.com/ZealanL/RocketSim) — Original C++ implementation
- [mtheall/RocketSim](https://github.com/mtheall/RocketSim) — Python bindings inspiration
