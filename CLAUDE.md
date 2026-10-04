# CLAUDE.md

Memoria de trabajo del proyecto. **Léela al empezar cada sesión y actualízala al terminar** cualquier cambio relevante (arquitectura, convenciones, decisiones, tareas pendientes). Debe ser concisa: aquí va lo que no se deduce rápido leyendo el código. El detalle técnico está en [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), que también hay que mantener al día.

## Qué es

Juego 3D en C++ sobre un motor propio (OpenGL 3.3 core). El objetivo es construir el motor y su arquitectura a la vez que el juego. Escena actual: un desierto nocturno (dunas, cactus, rocas, cielo con luna y estrellas, una "criatura" que respira a lo lejos) y un pingüino animado que se controla en tercera persona.

## Compilar, ejecutar, visualizar

```bash
cd src && make                  # genera ../test/test
../test/test --windowed [escena]   # se puede lanzar desde cualquier directorio
python3 tools/scene_viewer/serve.py                       # visor web interactivo
python3 tools/scene_viewer/serve.py --shot /ruta/x.png --view player|top|orbit
```

- **Para ver la escena tras un cambio, usa `--shot` y lee el PNG**. No hace falta lanzar el juego: genera la imagen con Firefox headless en ~10 s.
- Para comprobar el juego de verdad sin molestar mucho: lánzalo con `timeout 12 ../test/test --windowed &`, captura su ventana con `xwd -id $(wmctrl -l | grep "test bimbow" | awk '{print $1}')` y convierte el XWD con un script numpy (PIL no lee XWD).
- Dependencias: GLFW3, GLEW, GL, Assimp, GLM; Vulkan está enlazado pero no se usa. `stb_image` va incluido.
- No hay tests automáticos. `test.cpp` es el `main` del juego (el nombre es histórico). Para probar el parser de escenas sin GL, compila `SceneFile.cpp` con un `main` mínimo.

## Arquitectura (resumen)

- `test.cpp` crea ventana, `Shader`, `Camera`, `Controller`, `Scene` y `Light`, y ejecuta el bucle.
- **La disposición del mundo está en `assets/scenes/*.scene`** (datos). La leen `SceneFile` (C++) y `tools/scene_viewer/viewer.js`. Para añadir o mover objetos, edita el `.scene`, no el C++.
- `Scene` carga cada modelo una sola vez y dibuja el cielo, los objetos (con su efecto `lit|emissive|breathe`) y el jugador.
- Un solo shader: `animatedshader.vert` + `shader.frag`. `shader.vert` está en desuso.
- **Trampa:** en el shader, `model` es la *rotación de la cámara*, no la matriz del objeto. El objeto usa `objposition` + `objrotation` (que incluye la escala).
- Uniforms de control: `skinned`, `unlit` (0 iluminado, 1 cielo, 2 emisivo), `breathAmp`/`breathTime`, `fitCenter`/`fitScale`. Tabla completa en ARCHITECTURE.md.

## Puntos que deben estar sincronizados

- `SceneFile::load` ↔ `parseScene` en `viewer.js`.
- `animatedshader.vert`/`shader.frag` ↔ los shaders copiados en `viewer.js`.
- `moon` del `.scene` ↔ `MOON_DIR` en `assets/sky/generate_sky.py`. `fog` ↔ `HORIZON`.
- Altura `y` de los objetos del desierto = `-1 + dune_height(x, z) - 0.05` (`assets/desert/generate_assets.py`).
- Constantes del visor (`FOV`, `SENSIVILITY`, `PLAYER_HEIGHT`, `BREATH_AMPLITUDE`) ↔ `Camera`, `Controller`, `AnimatedModel`, `Scene.cpp`.

## Assets

- `desert/`, `sky/` y `creature/` se generan con `generate_*.py` (numpy + Pillow, con semilla). Se edita el script, no el OBJ.
- `ping/PenguinoAnimado.fbx` es el jugador; el `.original.fbx` es la copia sin tocar. `backpack/` no se usa.

## Estilo

- C++11 y `-Wall` sin avisos (mantenerlo así; `stb_image` se compila con `-w`), guardas `#ifndef`, clases en `PascalCase.h/.cpp`. El código nuevo usa 2 espacios; el antiguo usa tabs (respeta el de cada fichero).
- Comentarios del código en inglés. Documentación y conversación con el usuario en español.
- Cada cabecera tiene un comentario de clase; manténlo al día si cambia la responsabilidad de la clase.

## Deuda técnica

- `AnimatedMesh` duplica `Mesh` (TODO de herencia). `GameObject` usa un flag `anim` con dos punteros en vez de polimorfismo.
- `Light`/`Camera` escriben uniforms directamente; no hay un renderer como abstracción.
- No hay delta time, colisiones ni ajuste al terreno. Solo se reproduce la animación 0 (`Animation` está sin usar).
- No se liberan los recursos GL.
- Typos heredados: `SENSIVILITY`, `NUM_BONES_PER_VEREX`.

## Bitácora

Añadir una línea por sesión o cambio importante (AAAA-MM-DD).

- 2026-10-04: creado CLAUDE.md (estado en el commit `dfe494b`: animaciones del pingüino funcionando, cámara en tercera persona con `Controller`).
- 2026-10-04: la escena pasa a datos (`assets/scenes/desert.scene` + `SceneFile` + `Scene`), y `test.cpp` se simplifica. Se añaden el visor web `tools/scene_viewer/` (three.js, réplica de los shaders, `--shot` para PNG headless), `docs/ARCHITECTURE.md`, los comentarios de clase en las cabeceras y el README. Verificado: el juego compila, carga la escena sin errores y su captura coincide con la vista "player" del visor (no se comparó con una captura de antes del cambio).

- 2026-10-04: `.gitignore` creado y artefactos de compilación fuera del índice. Arreglado el error `must write to gl_Position`: salía al lanzar el juego fuera de `src/`, porque no se encontraban los shaders y se compilaba una fuente vacía. Ahora `main` hace `chdir` a `src/` y `fileToString` falla con un mensaje claro. Otras mejoras: rutas de textura con `std::string` (antes había un buffer de 120 bytes), makefile con `-std=c++11 -Wall` en todos los .o y `mkdir -p ../test`, eliminado el constructor roto `Mesh(indices, textures)`, comprobación correcta de `mMaterialIndex`, `Model::scene` ya no queda colgando, sin avisos de compilación.

## Próximos pasos / ideas

(Rellenar según lo que se decida con el usuario.)
- Posibles: delta time, que el jugador siga la altura del terreno, varias animaciones (andar/parado) usando `Animation`, una clase Renderer.
