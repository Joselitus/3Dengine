# 3Dengine

Basic 3D engine: a small C++/OpenGL 3.3 engine and the game being built on top of it. The current scene is a desert at night, with a skinned penguin controlled in third person.

## Build and run

Needs GLFW3, GLEW, Assimp and GLM (Linux).

```bash
cd src
make
../test/test --windowed             # or fullscreen without the flag
../test/test --windowed my.scene    # another scene file (default assets/scenes/desert.scene)
```

It can be launched from any directory: at startup it moves to `src/` (found next to the binary), where the shader and asset paths are relative to.

Controls: mouse looks around, WASD walks, Space/Shift move up/down, Esc quits.

## View a scene without running the game

```bash
python3 tools/scene_viewer/serve.py                          # interactive, in the browser
python3 tools/scene_viewer/serve.py --shot out.png --view player   # render to a PNG
```

See [tools/scene_viewer/README.md](tools/scene_viewer/README.md).

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): modules, frame loop, shader contract, scene file format, asset pipeline, how-tos.
- [CLAUDE.md](CLAUDE.md): working notes and log.
