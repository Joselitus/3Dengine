# 3Dengine

Basic 3D engine: a small C++/OpenGL 3.3 engine and the game being built on top of it. Two maps: a desert by day (an 8 m wide road making a closed loop around the start, cacti and rocks, a satellite you can aim, a talking penguin and a sign to read) and the desert at night (stars, moon, a creature). You always play as a penguin in first person; by day you can get into the RV (walk up to its door and press E) and drive it from outside, in third person, with a real spring suspension. It is fast on the road but slow on the sand (the floor knows what it is made of: asphalt or sand, and its wheels kick up a dust cloud there) (it bounces over the dunes, can roll over, and rights itself like a roly-poly toy). Left Shift gets you out again, at the door. Objects collide with each other and with the floor. The day desert has a looping background track (a guitar and a banjo; it is generated, see `assets/music/`). Press Z for the debug map selector.

## Source layout

`src/` is split by subsystem: `render/` (meshes, models, animation, camera, shader), `physics/` (collision shapes, vehicle body), `world/` (game objects, stages), `entities/` (RV, walker, NPC, satellite, readable sign), `input/`, `audio/`, `dialogue/`, `ui/` (widgets and menus), `core/` (settings), `shaders/` and `third_party/`. `src/test.cpp` is the `main`. The makefile picks up any `.cpp` in any of them and adds each folder to the include path, so a new file only needs to be put in the right folder.

## Build and run

Needs GLFW3, GLEW, Assimp and GLM (Linux). The NPCs' voices need `espeak-ng` installed (without it they show their text silently). Audio goes through miniaudio (included), using PulseAudio/PipeWire or ALSA.

```bash
cd src
make
../test/test --windowed             # or fullscreen without the flag
../test/test --windowed --ray       # floor lookup by downward ray instead of height field
```

It can be launched from any directory: at startup it moves to `src/` (found next to the binary), where the shader and asset paths are relative to.

Controls: mouse looks around, WASD walks (the floor keeps you on the dunes; in the RV, W/S drive and A/D steer; Left Shift gets out of the RV; keys can be changed in Options > Controls), E uses the object in front (the satellite: its panel sets azimuth and zenith; Pingu, an NPC who talks to you out loud with subtitles; a sign you can read), Esc closes a panel, or opens the pause menu (Resume; Options: Camera — mouse sensitivity and field of view —, Controls — rebind the keys — and Audio — master volume —, each with Save/Exit; Exit, or the X key, quits). Saved options go to `~/.config/3dengine/settings.cfg`. Z opens the debug map selector.

In the two forest maps ("Bosque" and "Bosque de pinos"), at night, a flying saucer lands and **Bob**, a grey alien with glowing white eyes, comes out of it. Keep away from him on foot: his eyes shoot a yellow ray that paralyses you, and if he catches you paralysed he takes you to his ship; if he catches you otherwise, hammer Left Shift to break free (he falls over). The ramp of the landed saucer comes down when Bob is near it: then you can get in (E) and fly it (R starts the engine, WASD moves, Space goes up, Left Shift goes down, Q draws the legs in or out; Left Shift gets you out only when it stands on its legs). At dawn Bob goes back aboard and the saucer flies away.

## View a scene without running the game

The viewer shows maps written as `.scene` files (the night desert, `assets/scenes/desert.scene`); maps built in code (the day desert) are not shown.

```bash
python3 tools/scene_viewer/serve.py                          # interactive, in the browser
python3 tools/scene_viewer/serve.py --shot out.png --view player   # render to a PNG
```

See [tools/scene_viewer/README.md](tools/scene_viewer/README.md).

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): modules, frame loop, floor and collisions, the RV's physics, shader contract, scene file format, asset pipeline, how-tos.
- [docs/UML.md](docs/UML.md): UML diagrams (Mermaid, also as PNG images in [docs/diagrams/](docs/diagrams/), starting with [the overview](docs/diagrams/01-vista-general.png)): an overview of the subsystems, class diagrams of every module, sequence diagrams of a frame, the physics step and a conversation, and an index of all the classes.
- [CLAUDE.md](CLAUDE.md): working notes and log.
