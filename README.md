# 3Dengine

Basic 3D engine: a small C++/OpenGL 3.3 engine and the game being built on top of it. Two maps: a desert by day (road, parked RV, a satellite you can aim) and the desert at night (stars, moon, a creature). You walk around as a penguin, in first person (head height). Press Z for the debug map selector.

## Build and run

Needs GLFW3, GLEW, Assimp and GLM (Linux). The NPCs' voices need `espeak-ng` installed (without it they show their text silently). Audio goes through miniaudio (included), using PulseAudio/PipeWire or ALSA.

```bash
cd src
make
../test/test --windowed             # or fullscreen without the flag
../test/test --windowed --ray       # floor lookup by downward ray instead of height field
```

It can be launched from any directory: at startup it moves to `src/` (found next to the binary), where the shader and asset paths are relative to.

Controls: mouse looks around, WASD walks (the floor keeps you on the dunes), E uses the object in front (the satellite: its panel sets azimuth and zenith; Pingu, an NPC who talks to you out loud with subtitles; a sign you can read), Esc closes a panel, or opens the pause menu (Resume; Options: mouse sensitivity, field of view and the list of all controls; Exit, or the X key, quits). Z opens the debug map selector.

## View a scene without running the game

The viewer shows maps written as `.scene` files (the night desert, `assets/scenes/desert.scene`); maps built in code (the day desert) are not shown.

```bash
python3 tools/scene_viewer/serve.py                          # interactive, in the browser
python3 tools/scene_viewer/serve.py --shot out.png --view player   # render to a PNG
```

See [tools/scene_viewer/README.md](tools/scene_viewer/README.md).

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): modules, frame loop, shader contract, scene file format, asset pipeline, how-tos.
- [CLAUDE.md](CLAUDE.md): working notes and log.
