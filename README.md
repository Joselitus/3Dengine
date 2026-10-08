# 3Dengine

Basic 3D engine: a small C++/OpenGL 3.3 engine and the game being built on top of it, now **multiplayer**: a server that runs the world and clients that connect to it over the network (see below). Five maps: a desert by day (an 8 m wide road making a closed loop around the start, cacti and rocks, a satellite you can aim, a talking penguin, a sign to read, a giant mosquito and a night creature), a forest road, Route 66 (20 km with a gas station in the middle), the desert at night (stars, moon, a creature) and a pine forest. Everyone plays as a penguin in first person; you can get into the RV (walk up to its door and press E, walk in, and press E at the wheel) and drive it, with a real spring suspension. It is fast on the road but slow on the sand (it bounces over the dunes, can roll over, and rights itself like a roly-poly toy). Left Shift gets you out again. Objects collide with each other and with the floor.

## Source layout

`src/` is split by subsystem: `render/` (meshes, models, animation, camera, shader), `physics/` (collision shapes, vehicle body), `world/` (game objects, stages), `entities/` (RV, walker, NPC, satellite, readable sign), `input/`, `audio/`, `dialogue/`, `ui/` (widgets and menus), `core/` (settings), `shaders/` and `third_party/`. `src/server.cpp` and `src/client.cpp` are the two `main`s (and `net/` is the network). The makefile builds both. The makefile picks up any `.cpp` in any of them and adds each folder to the include path, so a new file only needs to be put in the right folder.

## Build and run

Needs GLFW3, GLEW, Assimp and GLM (Linux). The NPCs' voices need `espeak-ng` installed (without it they show their text silently). Audio goes through miniaudio (included), using PulseAudio/PipeWire or ALSA.

```bash
cd src
make                                  # builds ../test/server, ../test/client and ../test/creature_testing

# on the machine that hosts the game (no window, no graphics needed at run time):
../test/server                        # the desert by day, port 7777
../test/server --map "Bosque de pinos" --port 7777   # another map (--list shows them)

# on every player's machine:
../test/client --windowed             # asks for the server's IP; or fullscreen without the flag
../test/client --windowed --connect 192.168.1.20 --name Ana   # skip the question
```

The client first asks for the **server's address** (an IP or a name, and `:port` if it is not 7777) and the name you go by. If it fails (nobody there, the server is full, a different version) it tells you why and asks again. The server is a plain TCP service: open its port (7777 by default) in the firewall for others to reach it. Up to 16 players. Both programs can be launched from any directory: at startup they move to `src/` (found next to the binary), where the shader and asset paths are relative to. Server options: `--port`, `--map N|NAME`, `--time H` (hour the day starts at), `--day-duration S` (seconds per day, 0 = stopped), `--ray`, `--verbose`. Client options: `--windowed`, `--connect ADDRESS`, `--name NAME`, `--ray`, `--profile`.

The server rules the world (the RV, the creatures, Bob, the day, who dies); the client draws it and sends what you do. Walking is predicted on your machine, so it answers at once; driving shows what the server says. Other players are the penguins you see, with their names above. If you die (or Bob takes you) you start again at the spawn point after a few seconds. In the console (T or /) `/day`, `/night` and `/time H` are sent to the server. The details are in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#multijugador-servidor-y-cliente).

Controls: mouse looks around, WASD walks (the floor keeps you on the dunes; in the RV, W/S drive and A/D steer; Left Shift gets out of the RV; keys can be changed in Options > Controls), E uses the object in front (the satellite: its panel sets azimuth and zenith; Pingu, an NPC who talks to you out loud with subtitles; a sign you can read), Esc closes a panel, or opens the pause menu (Resume; Options: Camera — mouse sensitivity and field of view —, Controls — rebind the keys — and Audio — master volume —, each with Save/Exit; Exit, or the X key, quits). Saved options go to `~/.config/3dengine/settings.cfg`.

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
