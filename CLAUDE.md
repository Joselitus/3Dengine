# CLAUDE.md

Memoria de trabajo del proyecto. **Léela al empezar cada sesión y actualízala al terminar** cualquier cambio relevante (arquitectura, convenciones, decisiones, tareas pendientes). Debe ser concisa: aquí va lo que no se deduce rápido leyendo el código. El detalle técnico está en [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), que también hay que mantener al día.

## Qué es

Juego 3D en C++ sobre un motor propio (OpenGL 3.3 core). El objetivo es construir el motor y su arquitectura a la vez que el juego. Nivel actual (`TestStage`, en `test.cpp`): un desierto de día con dunas, una carretera, cactus, rocas, una autocaravana aparcada y un satélite orientable. El jugador es un pingüino a pie, en primera persona a la altura de la cabeza. El desierto nocturno con la criatura es anterior y solo queda en `desert.scene` y en el visor.

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
- **Para probar la interfaz sin molestar al usuario:** (ojo: el ratón real que pase por encima de la ventana de Xephyr también llega al juego) no lances el juego en su pantalla (puede estar usándola y se mezclan las entradas). Usa un X anidado, `Xephyr :57 -screen 760x660 -ac &`, con `DISPLAY=:57 LIBGL_ALWAYS_SOFTWARE=1`. Envía teclas y ratón con python-xlib (`Xlib.ext.xtest.fake_input`), captura con `xwd -display :57 -id <ventana>` y busca la ventana por su título con `query_tree` (allí no hay gestor de ventanas). Al terminar, cierra Xephyr. Si un script falla a mitad, suelta los botones que haya pulsado y mata el juego que haya dejado abierto.
- No hay tests automáticos. `test.cpp` es el `main` del juego (el nombre es histórico). Para probar el parser de escenas sin GL, compila `SceneFile.cpp` con un `main` mínimo.

## Estado tras el merge con `Scene` (4 oct 2026)

`test.cpp` ahora usa **`Stage`** (abstracta, con `FloorMode::HeightField|DownwardRay`, `--ray` para el segundo) en vez de `Scene`: `TestStage` carga el desierto en C++, el jugador es un `RV` (`PlayableCharacter` abstracta -> `DynamicGameObject` -> `GameObject`, que ahora comparte `shared_ptr<Model>` y dibuja sus partes). Es de día (cielo azul liso, sin skydome), hay una carretera (`road.obj`) y la criatura está comentada. `Scene`/`SceneFile`/`desert.scene` y el visor web **siguen compilando pero ya no los usa el juego**: están desfasados (noche, pingüino) hasta decidir si `Stage` lee `.scene` o se retiran. `GameObject` ya no tiene los constructores con `Model*`.

## Arquitectura (resumen)

**`main` manda** (es el código de la otra persona). Lo nuestro se construye heredando de sus clases, no al revés.

- **Jerarquía de objetos:** `GameObject` (piezas `Part` compartidas o un `AnimatedModel`; posición, rotación, escala; `update(dt)` virtual; `setVisible`). De él deriva `DynamicGameObject` (velocidad, gravedad, `steerTowards`), y de este `PlayableCharacter` (abstracta: `control`, `attachCamera`, `followCamera`), que implementan `Walker` (el jugador) y `RV`.
- **`Stage`** (abstracta) es dueño de los objetos (`add`, `addDynamic`) y del suelo (`setFloor`, `floorAt`, `collideWithFloor`). **`TestStage`**, en `test.cpp`, monta el nivel en código. Un objeto nuevo se añade allí.
- **Cámara:** `Controller` → `PlayableCharacter::attachCamera(camera, distancia, altura)`. La distancia 0 es primera persona: `Walker` se oculta. Hoy es `attach(player, 0, 1.6)`, con la posición del jugador en sus pies.
- `Scene`/`SceneFile`/`.scene` y el visor web **ya no reflejan el juego** (son del diseño anterior). Siguen compilando, pero están pendientes de decidir.
- Un solo shader para el mundo: `animatedshader.vert` + `shader.frag`. **Trampa:** en el shader, `model` es la *rotación de la cámara*, no la matriz del objeto. El objeto usa `objposition` + `objrotation` (que incluye la escala).
- Uniforms de control: `skinned`, `unlit` (0 iluminado, 1 cielo, 2 emisivo; por pieza), `breathAmp`/`breathTime`, `fitCenter`/`fitScale`. Tabla completa en ARCHITECTURE.md.
- **Interfaz 2D propia** (`UI*`):
  - `UIElement` es la base y de ella derivan `UILabel`, `UIButton`, `UISlider` y `UIContainer` (de la que salen `UIPanel` y `UIRow`).
  - `UIManager` gestiona los paneles y el ratón. `UIRenderer` usa su propio shader y deja activo de nuevo el del motor al terminar.
  - El texto usa `stb_easy_font`, que **solo admite ASCII**, así que los textos van sin tildes.
- **Objetos usables:** heredan de `Interactable` y rellenan su panel en `buildInterface`. `TestStage::getInteractables()` los registra en el `InteractionSystem`, que muestra el aviso "E: usar …" y abre o cierra el panel (E/Esc), pausando el `Controller` (`setEnabled`, que además le da al personaje una entrada nula).
- `Satellite`: la cabeza es el objeto y el poste un `GameObject` aparte (`getMount()`), porque todas las piezas de un `GameObject` comparten la misma transformación. Azimut desde −z en sentido horario hacia +x; cénit 0 = vertical. Gira hacia el objetivo a `slewRate` °/s.

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

- 2026-10-04: **ramas.** `main` (= `origin/main`, `4b7d902`) incluye el merge de `bc28e6d`, de otra persona: `Stage` abstracta con seguimiento del suelo, `DynamicGameObject`, `PlayableCharacter`, `RV`, carretera y autocaravana. Allí `test.cpp` monta `TestStage` en código, así que `Scene`/`.scene` y el visor no corresponden a lo que hace el juego. Ese merge también volvió a versionar `.o` y binarios; el `test/test` de esa persona no arranca aquí (glibc 2.43, GLEW 2.3, Assimp 6). **Recompila siempre tras un pull.** El usuario trabaja ahora en la rama `scayuelas` (`41e98df`, sin `Stage`). Los cambios que había sin commit en `main` quedaron en un autostash.
- 2026-10-04 (rama `scayuelas`): sistema de interfaz genérico (`UI*`, `Interactable`, `InteractionSystem`), `Satellite` y asset `assets/cube/` (con `generate_cube.py`). `GameObject` pasa a tener `Update`/`Draw` virtuales y `Controller` incluye `setEnabled` y el armado de Esc. Verificado de extremo a extremo en Xephyr con XTest: aparece el aviso, E abre el panel, los deslizadores giran el cubo (az 90 / cénit 45 → dirección (0.71, 0.71, 0)), se ve el estado "girando", Esc cierra solo el panel y el segundo Esc sale, y el botón X cierra. Pendiente decidir cómo llevar esto a `main` (que tiene `Stage`).

- 2026-10-04 (`scayuelas`): cámara en **primera persona** (`camera 0 0.7` en `desert.scene`). Con distancia 0, `Camera::follow` ya coloca la cámara en los ojos, que gira sobre sí misma. El jugador se oculta con `Scene::setPlayerVisible` y el visor hace lo mismo en su vista "Jugador". Verificado en Xephyr (vista a la altura de los ojos, aviso y panel del satélite funcionando) y en el visor. En una ejecución en Xephyr la cámara apareció mirando al suelo y luego al cielo, pero no se reprodujo: sin entrada, al dar el foco o al pulsar teclas, la cámara no se mueve. Causa probable, según el usuario: él estaba usando el ratón a la vez. Xephyr es una ventana del escritorio y reenvía al juego el ratón real cuando pasa por encima. **Si una prueba interactiva da resultados raros, avisa al usuario y pídele el control del ratón y el teclado antes de repetirla.**

- 2026-10-04 (`main`): merge de `scayuelas` (`5033f39`) en `main`, priorizando `main`. Los compilados siguen fuera de git. `GameObject` y `Controller` parten de `main`: al primero se le añade `setVisible` y al segundo `setEnabled`, con Esc armado y entrada nula al pausar. `Satellite` se reescribe sobre el `GameObject` de `main` (`shared_ptr`, `update(double)`, poste aparte). La primera persona se rehace como `Walker : PlayableCharacter` (decisión del usuario: el jugador es el pingüino a pie y la autocaravana queda aparcada). `TestStage` añade el jugador, el satélite (sobre `floorAt`) y la lista de interactuables. Compila sin avisos desde cero. Verificado en Xephyr: primera persona, aviso, panel, giro a az 90 / cénit 45, Esc cierra solo el panel, W anda y el segundo Esc sale.

## Próximos pasos / ideas

(Rellenar según lo que se decida con el usuario.)
- Decidir el futuro de `.scene`/`Scene`/visor web: que `Stage` cargue `.scene`, o retirarlos.
- Entrar en la autocaravana (que sea `Interactable` y cambie el `PlayableCharacter` del `Controller`).
- Modelo del pingüino apoyado por los pies (hoy `AnimatedModel` lo centra en su posición); solo afecta a la tercera persona.
- Posibles: rueda del ratón o teclado en la UI, satélite en el `.scene` y en el visor, delta time, que el jugador siga la altura del terreno, varias animaciones (andar/parado) usando `Animation`, una clase Renderer.
