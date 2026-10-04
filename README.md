# 3Dengine

Basic 3D engine: a small C++/OpenGL 3.3 engine and the game being built on top of it. The current level is a desert by day with a road, a parked RV and a satellite you can aim; you walk around as a penguin, in first person (head height).

## Build and run

Needs GLFW3, GLEW, Assimp and GLM (Linux).

```bash
cd src
make
../test/test --windowed             # or fullscreen without the flag
../test/test --windowed --ray       # floor lookup by downward ray instead of height field
```

It can be launched from any directory: at startup it moves to `src/` (found next to the binary), where the shader and asset paths are relative to.

Controls: mouse looks around, WASD walks (the floor keeps you on the dunes), E uses the object in front (e.g. the satellite: its panel sets azimuth and zenith), Esc closes a panel or quits.

## View a scene without running the game

Note: the viewer reads `.scene` files, which the game no longer uses since the `Stage` rework; it shows the older night-time desert.

```bash
python3 tools/scene_viewer/serve.py                          # interactive, in the browser
python3 tools/scene_viewer/serve.py --shot out.png --view player   # render to a PNG
```

See [tools/scene_viewer/README.md](tools/scene_viewer/README.md).

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): modules, frame loop, shader contract, scene file format, asset pipeline, how-tos.
- [CLAUDE.md](CLAUDE.md): working notes and log.
