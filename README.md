# RocketSimPy

**A modernized fork of [RocketSim](https://github.com/ZealanL/RocketSim) with official Python bindings**

A C++ library for simulating Rocket League games at maximum efficiency, now with first-class Python support via [nanobind](https://github.com/wjakob/nanobind).

## What's Different

This fork builds on ZealanL's original RocketSim and takes inspiration from [mtheall's Python bindings](https://github.com/mtheall/RocketSim):

- **nanobind bindings** — Faster, cleaner Python bindings (not pybind11)
- **Full test coverage** — C++ and Python test suites with CI
- **RLGym compatible** — Drop-in replacement for [rlgym](https://rlgym.org/) environments
- **Modern build system** — scikit-build-core + uv for Python packaging

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

RocketSim simulates ~20 minutes of game time per second on a single thread. With 12 threads, that's ~10 days of game time per minute.

## Accuracy

Accurate enough to train ML bots to SSL level, simulate shots, air control, and pinches. Small errors accumulate over time — best suited for simulation with consistent feedback.

## Installation

1. Clone this repo
2. Dump arena collision meshes using [RLArenaCollisionDumper](https://github.com/ZealanL/RLArenaCollisionDumper)
3. Build: `mkdir build && cd build && cmake .. && make`

For Python bindings, see [python/README.md](python/README.md).

## Documentation

- Original docs: [zealanl.github.io/RocketSimDocs](https://zealanl.github.io/RocketSimDocs/)
- Python API: [python/README.md](python/README.md)

## Credits

- [ZealanL/RocketSim](https://github.com/ZealanL/RocketSim) — Original implementation
- [mtheall/RocketSim](https://github.com/mtheall/RocketSim) — Python bindings inspiration
- [RLGym](https://rlgym.org/) — Target compatibility

## Legal Notice

RocketSim replicates Rocket League's game logic but contains no code from the game.
