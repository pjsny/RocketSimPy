# RocketSimPy

A fork of [RocketSim](https://github.com/ZealanL/RocketSim) with Python bindings.

RocketSim is a C++ library that simulates Rocket League physics. This fork adds [nanobind](https://github.com/wjakob/nanobind) bindings so you can use it from Python.

## What's different from upstream

Built on ZealanL's RocketSim, with ideas from [mtheall's Python bindings](https://github.com/mtheall/RocketSim):

- nanobind instead of pybind11 (smaller, faster builds)
- C++ and Python test suites with CI
- Works with [rlgym](https://rlgym.org/)
- scikit-build-core + uv for packaging

## Quick Start

```bash
# Build and install Python bindings
cd python
uv build --wheel
uv pip install dist/*.whl
```

```python
import RocketSim as rs

rs.init("collision_meshes")
arena = rs.Arena(rs.GameMode.SOCCAR)
car = arena.add_car(rs.Team.BLUE, rs.CAR_CONFIG_OCTANE)
arena.step(100)
```

## Speed

About 20 minutes of game time per second on one thread. 12 threads gets you ~10 days of game time per minute.

## Accuracy

Good enough to train ML bots to SSL, simulate shots, air control, pinches. Errors accumulate over time so it works best when you're running short simulations with frequent resets.

## Building

1. Clone this repo
2. Get arena collision meshes with [RLArenaCollisionDumper](https://github.com/ZealanL/RLArenaCollisionDumper)
3. Build: `mkdir build && cd build && cmake .. && make`

For Python, see [python/README.md](python/README.md).

## Docs

- Original RocketSim docs: [zealanl.github.io/RocketSimDocs](https://zealanl.github.io/RocketSimDocs/)
- Python API: [python/README.md](python/README.md)

## Credits

- [ZealanL/RocketSim](https://github.com/ZealanL/RocketSim)
- [mtheall/RocketSim](https://github.com/mtheall/RocketSim)
- [RLGym](https://rlgym.org/)

## Legal

RocketSim replicates Rocket League physics but doesn't contain any code from the game.
