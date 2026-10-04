# CLAUDE.md

Memoria de trabajo del proyecto. **Léela al empezar cada sesión y actualízala al terminar** cualquier cambio relevante (arquitectura, convenciones, decisiones, tareas pendientes). Debe ser concisa: aquí va lo que no se deduce rápido leyendo el código. El detalle técnico está en [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), que también hay que mantener al día.

## Qué es

Juego 3D en C++ sobre un motor propio (OpenGL 3.3 core). El objetivo es construir el motor y su arquitectura a la vez que el juego. Dos mapas, que se cambian con el selector de depuración (tecla Z):
- **"Desierto de dia"** (`TestStage`, en código en `test.cpp`): dunas, carretera, cactus, rocas, un satélite orientable, un cartel, un NPC (Pingu) y un pingüino a pie. **El jugador es el `RV`** (autocaravana con suspensión), en tercera persona.
- **"Desierto de noche"** (`SceneStage` + `assets/scenes/desert.scene`): cielo estrellado, luna y la criatura.

En el de noche el jugador es un pingüino a pie (`Walker`), en primera persona a la altura de la cabeza.

## Compilar, ejecutar, visualizar

```bash
cd src && make                  # genera ../test/test
../test/test --windowed [escena]   # se puede lanzar desde cualquier directorio
python3 tools/scene_viewer/serve.py                       # visor web interactivo
python3 tools/scene_viewer/serve.py --shot /ruta/x.png --view player|top|orbit
```

- **Para ver la escena tras un cambio, usa `--shot` y lee el PNG**. No hace falta lanzar el juego: genera la imagen con Firefox headless en ~10 s.
- Para comprobar el juego de verdad sin molestar mucho: lánzalo con `timeout 12 ../test/test --windowed &`, captura su ventana con `xwd -id $(wmctrl -l | grep "test bimbow" | awk '{print $1}')` y convierte el XWD con un script numpy (PIL no lee XWD).
- Dependencias: GLFW3, GLEW, GL, Assimp, GLM; Vulkan está enlazado pero no se usa. `stb_image` va incluido (en `src/third_party/`).
- **Para probar la interfaz sin molestar al usuario:** (ojo: el ratón real que pase por encima de la ventana de Xephyr también llega al juego) no lances el juego en su pantalla (puede estar usándola y se mezclan las entradas). Usa un X anidado, `Xephyr :57 -screen 760x660 -ac &`, con `DISPLAY=:57 LIBGL_ALWAYS_SOFTWARE=1`. Envía teclas y ratón con python-xlib (`Xlib.ext.xtest.fake_input`), captura con `xwd -display :57 -id <ventana>` y busca la ventana por su título con `query_tree` (allí no hay gestor de ventanas). Al terminar, cierra Xephyr. Si un script falla a mitad, suelta los botones que haya pulsado y mata el juego que haya dejado abierto.
- **Organización de `src/`:** `render/` (mallas, modelos, animación, Shader, Camera), `physics/` (CollisionShape, VehicleBody), `world/` (GameObject, Stage, GameStage, SceneStage...), `entities/` (RV, Walker, Npc, Satellite, Readable), `input/`, `audio/`, `dialogue/`, `ui/` (UI* y menús), `core/` (Settings), `shaders/`, `third_party/`; `test.cpp` (el `main`) y el makefile quedan en `src/`. El makefile compila cualquier `.cpp` y pone cada subdirectorio como include path, así que los `#include` son por nombre (`#include "Stage.h"`). Un fichero nuevo va en el subdirectorio que corresponda; no hay que tocar el makefile. Los shaders se abren como `shaders/...` (el directorio de trabajo es `src/`).
- **Diagramas ([docs/UML.md](docs/UML.md)):** son Mermaid. Para validarlos: en un directorio temporal, `npm i mermaid jsdom` y un script que cree un `JSDOM`, lo ponga en `globalThis.window/document` y llame a `mermaid.parse(bloque)` por cada ```` ```mermaid ````. Para verlos: `npm i @mermaid-js/mermaid-cli` con `PUPPETEER_SKIP_DOWNLOAD=1` y `mmdc -p cfg.json -i bloque.mmd -o salida.png`, con un `cfg.json` `{"executablePath":"/usr/bin/google-chrome-stable","args":["--no-sandbox"]}`. Si cambia una herencia o un dueño, actualiza el diagrama y el índice del final.
- No hay tests automáticos. `test.cpp` es el `main` del juego (el nombre es histórico). Para probar el parser de escenas sin GL, compila `SceneFile.cpp` con un `main` mínimo.

## Estado actual: mundo, física y colisiones

`test.cpp` monta los mapas sobre **`Stage`** (abstracta, con `FloorMode::HeightField|DownwardRay`; `--ray` elige el segundo): `TestStage` (día, en C++) y `SceneStage` (noche, desde `assets/scenes/desert.scene` con `SceneFile`). Los dos heredan de `GameStage`. `GameObject` comparte `shared_ptr<Model>` y dibuja sus piezas (cada una con transformación local opcional). El visor web y `.scene` solo cubren la noche.

- **Jerarquía:** `GameObject` → `DynamicGameObject` (velocidad, masa, gravedad; ganchos `contactFloor` y `applyCollision`) → `PlayableCharacter` (abstracta) → `Walker` / `RV`; también `Npc` (dinámico) y `Satellite`/`Readable` (estáticos), los tres `Interactable`.
- **Colisiones (en `Stage`):** cada `GameObject` tiene una `CollisionShape` (`physics/CollisionShape.h`): por defecto una `Capsule` ajustada al modelo, o la que se pase **al construir** (el RV usa una `Box`). Rejilla fija de celdas (8 m, 2.º argumento del constructor de `Stage`): solo se comparan los objetos que comparten celda. Los objetos no se solapan (se mueve el dinámico; entre dos, el más ligero) y la forma de un dinámico no atraviesa el suelo **de ningún lado** (`floorSamples`: esquinas y cara más baja en el mundo). `setCollidable(false)` en el suelo y la carretera, **antes de `add()`**: un estático se registra al añadirlo y no debe moverse después.
- **El RV** es un `VehicleBody` (`physics/VehicleBody.h`, sin OpenGL): chasis rígido sobre 4 muelles con recorrido limitado, neumáticos con círculo de fricción, **centro de masas bajo los ejes** y un par de autoenderezado tipo tentetieso (vuelve solo a las ruedas). Las ruedas son piezas aparte (`wheel_negx/posx.obj`) que siguen la suspensión. Las 8 esquinas del chasis son contactos elásticos *por fuera* de la caja de colisión: así la física actúa antes que la corrección dura del stage (si no, un RV volcado se desliza sin rozamiento). W/S aceleran, A/D giran; la dirección de la cámara se ignora.
- **Cada frame** (`Stage::update`): objetos → por cada dinámico `update`, `apply` (suelo; el RV lo hace en `contactFloor`) y forma-contra-suelo → `resolveCollisions` → forma-contra-suelo otra vez. Detalle y diagramas en ARCHITECTURE.md ("Suelo y colisiones", "El RV") y en [docs/UML.md](docs/UML.md).

## Arquitectura (resumen)

**`main` manda** (es el código de la otra persona). Lo nuestro se construye heredando de sus clases, no al revés.

- **Jerarquía de objetos:** `GameObject` (piezas `Part` compartidas o un `AnimatedModel`; posición, rotación, escala; `update(dt)` virtual; `setVisible`). De él deriva `DynamicGameObject` (velocidad, gravedad, `steerTowards`), y de este `PlayableCharacter` (abstracta: `control`, `attachCamera`, `followCamera`), que implementan `Walker` (el jugador de la noche, a pie) y `RV` (el del día).
- **`Stage`** (abstracta) es dueño de los objetos (`add`, `addDynamic`), del suelo (`setFloor`, `floorAt`, `collideWithFloor`) y de la rejilla de colisiones. Cada escenario implementa `apply(objeto, dt)`.
- **`GameStage : Stage`** (abstracta, nuestra) es un mapa jugable: `Environment` (luz y horizonte), cielo opcional, jugador y cámara, interactuables y `render()`. De ella derivan `TestStage` (día, en código) y `SceneStage` (cualquier `.scene`).
- **Mapas:** la lista `maps` de `main()` (nombre + fábrica). Cambiar de mapa siempre pasa por `switchMap`, **diferido** al principio del frame (`requestedMap`), nunca dentro de un callback de la UI. `switchMap` cierra los paneles, vacía el `InteractionSystem`, cambia el stage, conecta el `Controller` y aplica el entorno. Nada puede guardar punteros a objetos del mapa sin limpiarlos ahí. Guía "Añadir un mapa" en ARCHITECTURE.md.
- **Cámara:** `Controller` → `PlayableCharacter::attachCamera(camera, distancia, altura)`. La distancia 0 es primera persona: `Walker` se oculta. Cada mapa pide la suya (`cameraDistance`/`cameraHeight`): a pie 0 y 1.6 (con la posición del jugador en sus pies); el RV, 12 y 3.5.
- **`.scene`**: mapas en datos. Los leen `SceneFile` → `SceneStage` y el visor web, cuyos parsers deben estar sincronizados. `floor` define el suelo, y una `y` = `ground` apoya el objeto en él; úsalo siempre, porque las dunas se regeneran. La clase `Scene` antigua se eliminó.
- Un solo shader para el mundo: `shaders/animatedshader.vert` + `shaders/shader.frag`. **Trampa:** en el shader, `model` es la *rotación de la cámara*, no la matriz del objeto. El objeto usa `objposition` + `objrotation` (que incluye la escala).
- Uniforms de control: `skinned`, `unlit` (0 iluminado, 1 cielo, 2 emisivo; por pieza), `breathAmp`/`breathTime`, `fitCenter`/`fitScale`. Tabla completa en ARCHITECTURE.md.
- **Interfaz 2D propia** (`UI*`):
  - `UIElement` es la base y de ella derivan `UILabel`, `UIButton`, `UISlider` y `UIContainer` (de la que salen `UIPanel` y `UIRow`).
  - `UIManager` gestiona los paneles y el ratón. `UIRenderer` usa su propio shader y deja activo de nuevo el del motor al terminar.
  - El texto usa `stb_easy_font`, que solo tiene ASCII: `UIRenderer::toAscii` quita las tildes al dibujar. Escribe los textos en español correcto (la voz los necesita así); en pantalla saldrán sin tildes.
- **Objetos usables:** heredan de `Interactable` y rellenan su panel en `buildInterface`. `TestStage::getInteractables()` los registra en el `InteractionSystem`, que muestra el aviso "E: usar …" y abre o cierra su panel con E.
- **Teclado y Esc (importante):** `UIManager` es **el único dueño** del *key callback* de GLFW y del *user pointer* de la ventana; no instales otros. Las teclas van al `onKey` del panel de arriba; Esc cierra ese panel si no la gestiona, y sin paneles se ejecuta su atajo (`ui.bindKey`): Esc → `PauseMenu`, Z → `MapSelector`. `Controller` ya no lee Esc.
- **Pausa de controles:** una sola regla en el bucle de `test.cpp`: `controller.setEnabled(!ui.hasPanels())`. No pauses el `Controller` desde paneles ni sistemas.
- **Teclas:** `Controls` es la única fuente (`Action` → tecla). **No escribas `GLFW_KEY_...` para una función del juego:** añade una `Action` y léela con `controls.key(...)`, para que salga en la pantalla de controles y se pueda reasignar. Solo Esc (el "atrás" de la UI) es fijo. Las teclas se reasignan en `ControlsMenu` (sobre una copia; Guardar, Salir con aviso `ConfirmDialog` si hay cambios) y se guardan como `controls.<id>`. Para atajos con una tecla de `Controls`, usa `ui.bindKey(función que devuelve la tecla, acción)`. Detalles en ARCHITECTURE.md, "Controles y teclas".
- **Configuración guardada:** `Settings` (`clave = valor` en `~/.config/3dengine/settings.cfg`, o `$XDG_CONFIG_HOME`). `CameraMenu::applySettings` y `Controls::readFrom` la aplican al arrancar. Solo se guarda con "Guardar" en las pantallas de ajustes. **En las pruebas usa `XDG_CONFIG_HOME=<temporal>`** para no pisar la configuración del usuario.
- **Menús** (todos son **subclases de `UIPanel`** y reciben un `MenuContext`: ui, camera, controls, settings, quit):
  - `PauseMenu`: Reanudar / Opciones / Salir; la tecla X sale.
  - `OptionsMenu`: una lista con Cámara, Controles y Volver.
  - `SettingsMenu` (abstracta): base de las pantallas de ajustes, con Por defecto / Guardar / Volver y aviso `ConfirmDialog` si hay cambios sin guardar. De ella heredan `CameraMenu` (sensibilidad y FOV, con vista previa en directo y descarte que repone) y `ControlsMenu` (reasigna teclas sobre una copia).
  - `MapSelector`: Z, debug. Para cualquier ventana nueva, sigue la guía "Crear una ventana nueva" de ARCHITECTURE.md: heredar de `UIPanel`, añadir hijos en el constructor, `onKey` para atajos, `closable = false` y `dimsBackground()` en menús, y `ui.open(...)` + `requestClose()` para pasar de una ventana a otra.
- **Cámara:** `Camera::setAngles(yaw, pitch)` en radianes, más `setFov`/`setSensitivity`. `Controller` acumula el giro a partir del desplazamiento del ratón en cada frame (antes usaba la posición absoluta del cursor, y cambiar la sensibilidad hacía saltar la cámara).
- **Audio y voz:** `SoundEngine` (miniaudio, cabecera única en `src/third_party/`) con `Sound`/`AudioClip`; el oyente sigue a la cámara cada frame. `SpeechSynthesizer` es abstracta y `EspeakSynthesizer` lanza `espeak-ng` con fork/exec, así que **espeak-ng es una dependencia en tiempo de ejecución**. `Voice` sintetiza con `std::async` y da `progress()` para los subtítulos (`UITextBlock`). `Npc : DynamicGameObject, Interactable` (de momento "Pingu" en `TestStage`).
- **Diálogos:** `Dialogue` es la caja común (páginas, "Siguiente"/"Cerrar", Esc) y usa un `LineNarrator`: `Voice` (TTS) en `Npc`, `Typewriter` (silencioso, letra a letra) en `Readable : GameObject, Interactable` (el "Cartel" de `TestStage`, `assets/sign/`). Para un objeto que se lee, usa `Readable`; para otra forma de entregar el texto, crea una subclase de `LineNarrator`. El narrador se declara antes que el `Dialogue`. `Interactable::getInteractionVerb()` da el verbo del aviso ("hablar con", "leer"). `Interactable` tiene ahora `onInterfaceOpened`/`onInterfaceClosed`. `AnimatedModel(path, feetAtOrigin)`. Todo está en la sección "Audio y voz" de ARCHITECTURE.md.
- **Para probar el audio:** usa siempre una salida virtual (`pactl load-module module-null-sink sink_name=engine_test`, juego con `PULSE_SINK=engine_test`, `parec --device=engine_test.monitor`) y descárgala al terminar. Grabar el monitor por defecto mezcla lo que el usuario escucha (le pasó: tenía audio sonando), y además así el usuario no oye las pruebas.
- `Satellite`: la cabeza es el objeto y el poste un `GameObject` aparte (`getMount()`), porque todas las piezas de un `GameObject` comparten la misma transformación. Azimut desde −z en sentido horario hacia +x; cénit 0 = vertical. Gira hacia el objetivo a `slewRate` °/s.

## Puntos que deben estar sincronizados

- `SceneFile::load` ↔ `parseScene` en `viewer.js`.
- `shaders/animatedshader.vert`/`shaders/shader.frag` ↔ los shaders copiados en `viewer.js`.
- Medidas del RV: `RV.cpp` (ejes de las ruedas 2.4 y −2.3, vía 1.2, radio 0.5, caja del chasis) ↔ `assets/rv/generate_rv.py`. Las ruedas salen de ese script como `wheel_negx/posx.obj` centradas en su eje.
- `moon` del `.scene` ↔ `MOON_DIR` en `assets/sky/generate_sky.py`. `fog` ↔ `HORIZON`.
- Altura `y` de los objetos del desierto = `-1 + dune_height(x, z) - 0.05` (`assets/desert/generate_assets.py`).
- Constantes del visor (`FOV`, `SENSIVILITY`, `PLAYER_HEIGHT`, `BREATH_AMPLITUDE`) ↔ `Camera`, `Controller`, `AnimatedModel`, `SceneStage`.

## Assets

- `desert/`, `sky/`, `creature/` y `rv/` se generan con `generate_*.py` (numpy + Pillow, con semilla). Se edita el script, no el OBJ.
- `ping/PenguinoAnimado.fbx` es el jugador; el `.original.fbx` es la copia sin tocar. `backpack/` no se usa.

## Estilo

- C++11 y `-Wall` sin avisos (mantenerlo así; `stb_image` se compila con `-w`), guardas `#ifndef`, clases en `PascalCase.h/.cpp`. El código nuevo usa 2 espacios; el antiguo usa tabs (respeta el de cada fichero).
- Comentarios del código en inglés. Documentación y conversación con el usuario en español.
- Cada cabecera tiene un comentario de clase; manténlo al día si cambia la responsabilidad de la clase.

## Deuda técnica

- `AnimatedMesh` duplica `Mesh` (TODO de herencia). `GameObject` usa un flag `anim` con dos punteros en vez de polimorfismo.
- `Light`/`Camera` escriben uniforms directamente; no hay un renderer como abstracción.
- Las colisiones entre objetos no tienen rebote ni rozamiento; solo separan. No hay botón de recolocar un RV volcado (aunque se endereza solo). Solo se reproduce la animación 0 (`Animation` está sin usar).
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

- 2026-10-04 (`main`): **menú de pausa y de opciones.** Esc → `PauseMenu` (Opciones, Salir, tecla X). `OptionsMenu` ajusta la sensibilidad (0.2–3x) y el FOV (40–110°). Cambios en la UI genérica: `onKey`, *key callback* en `UIManager`, Esc genérico con *escape handler*, `open(UIPanel*)`/`close`/`isOpen`, y `UIPanel` con `closable` y `dimsBackground`. `Camera` gana FOV y sensibilidad ajustables y ángulos en radianes (`setAngles`; se quitan `rotate(int,int)`, `getPhi` y `getTheta`). `Controller` pasa a usar el desplazamiento del ratón y deja de leer Esc. `InteractionSystem` ya no conoce el `Controller`. Verificado en Xephyr: pausa, opciones, FOV 90, sensibilidad 2x (60 px → 0.6 rad), Esc entre menús, satélite, salir con X y con el botón.

- 2026-10-04 (`main`): botón **Reanudar** en la pausa. **Selector de mapas de depuración** (Z) y mapa de noche recuperado. Se añaden `GameStage` (entre `Stage` y los mapas) y `SceneStage` (carga `.scene`); `TestStage` pasa a heredar de `GameStage` y se le mueve el entorno que estaba en `main()`. Formato `.scene`: `floor` y `y` = `ground`; `desert.scene` actualizado (cámara `0 1.6`, objetos `ground`); el visor entiende `floor`/`ground` (con un raycast hacia abajo). `UIManager::bindKey` sustituye a `setEscapeHandler`. `Light::setColor`, `InteractionSystem::clear` y `Controller::attach` reinician la vista. Se elimina la clase `Scene`. Verificado en Xephyr: Reanudar, Z, noche (cielo, criatura, andar), Z/Z, vuelta al día con el satélite, X; y el visor.

- 2026-10-04 (`main`): **pantalla de Controles** (Opciones → Controles), solo informativa. Registro central `Controls` (`Action` → tecla, `describe`, `group`, `keyName`, `fixedControls`); `Controller`, `InteractionSystem`, `PauseMenu`, `MapSelector` y `test.cpp` leen de él y ya no queda ninguna tecla del juego escrita a mano (solo Esc, que es fijo). Se añaden `MenuContext` (los menús reciben ui/camera/controls/quit) y `UIInfoRow`. El texto del panel del satélite pasa a "Esc: cerrar". Verificado en Xephyr: Esc → Opciones → Controles → Esc×3, W, Z/Z, X. Confirmado además que todo lo nuestro hereda de las clases de `main` sin modificarlas (`Stage`, `DynamicGameObject`, `PlayableCharacter`, `RV`).

- 2026-10-04 (`main`): **TTS + motor de sonido + NPC.** miniaudio 0.11.21 (`miniaudio.h`/`.cpp`, compilado con `-w`; makefile con `-pthread -ldl`). `SoundEngine`/`Sound`/`AudioClip`, `SpeechSynthesizer` → `EspeakSynthesizer` (voz `es`; las MBROLA no están instaladas; `/usr/bin/piper` es una aplicación de ratones, no TTS), `Voice` asíncrona, `Npc` "Pingu" en el mapa de día, `UITextBlock` (subtítulos que avanzan con la voz) y `UIRenderer::toAscii`. Verificado: el sintetizador funciona con tildes, comillas y `$(...)`; la grabación en una salida virtual muestra silencio → voz al pulsar E → fin de la frase → "Siguiente" → silencio inmediato con Esc. Subtítulos y NPC verificados en Xephyr.

- 2026-10-04 (`main`): diálogo del `Npc` sin "Repetir"; "Siguiente" pasa a "Cerrar" en la última frase y cierra el panel; cada conversación empieza en la frase 1; Esc corta la voz en cualquier momento. `UIButton` admite texto dinámico. Verificado en Xephyr con la salida virtual.

- 2026-10-04 (`main`): **sistema de lectura sin voz.** La lógica del diálogo sale de `Npc` a `Dialogue` + `LineNarrator` (`Voice` lo implementa; nuevo `Typewriter`). Se añaden `Readable` y un cartel en el mapa de día (`assets/sign/generate_sign.py`), y `getInteractionVerb`. Verificado en Xephyr con la salida virtual: el cartel se escribe solo, en silencio total, y sus tres páginas acaban en "Cerrar"; Pingu sigue hablando y Esc lo corta; los avisos dicen "E: hablar con Pingu" y "E: leer Cartel".

- 2026-10-04 (`main`): **las opciones se guardan entre sesiones.** Nueva clase `Settings` (fichero `clave = valor` en la carpeta de configuración del usuario, escritura atómica, conserva claves desconocidas). `OptionsMenu` aplica los valores al arrancar y guarda al cerrarse; `MenuContext` incluye `settings`. Verificado con una prueba unitaria de `Settings` (fichero nuevo, releer, líneas rotas) y con dos sesiones en Xephyr usando un `XDG_CONFIG_HOME` temporal: el FOV 90 y la sensibilidad 2x se conservan.

- 2026-10-04 (`main`): **teclas reasignables y guardadas.** Se quitan `MoveUp`/`MoveDown` ("sin gravedad"). `Controls` incorpora `id`, `actionFor`, `==`, `isBindable` y `readFrom`/`writeTo` en `Settings` (validando el conjunto: admite intercambios e ignora duplicados, Esc y basura). `ControlsMenu` reasigna (clic + tecla, intercambio si está ocupada) sobre una copia, con Por defecto / Guardar / Volver, y avisa con un `ConfirmDialog` ("Guardar y salir" / "Salir") si hay cambios sin guardar. `UIInfoRow` puede ser clicable. `UIManager::bindKey` admite una función que devuelve la tecla (los atajos siguen a los cambios). Verificado con una prueba unitaria de `readFrom` y con dos sesiones en Xephyr (intercambio, descartar, guardar, I avanza, se conserva al reiniciar, Guardar y salir).

- 2026-10-04 (`main`): **Opciones pasa a ser una lista** (Cámara / Controles / Volver). Nueva base `SettingsMenu` (Por defecto / Guardar / Volver + aviso de cambios sin guardar), de la que heredan `CameraMenu` (nuevo: sensibilidad y FOV, en directo, descartables) y `ControlsMenu`. Ya no se guarda nada al cerrar sin pulsar Guardar. Verificado en Xephyr: FOV 90 en directo, aviso, descartar (vuelve a 45), guardar (90 en el fichero, sin aviso al salir), Controles desde el menú nuevo.

- 2026-10-04 (`main`): **estructura de `src/` en subdirectorios** (`render/`, `physics/`, `world/`, `entities/`, `input/`, `audio/`, `dialogue/`, `ui/`, `core/`, `shaders/`, `third_party/`), con `git mv` (se conserva el historial). El makefile busca los `.cpp` con `find` y añade cada directorio como `-I`, así que los `#include` siguen siendo por nombre. Solo cambian las dos rutas de shader (`shaders/...`) en `test.cpp` y `UIRenderer.cpp`. Compila sin avisos desde cero y arranca desde `src/` y desde `/tmp`.
- 2026-10-04 (`main`): **suspensión del RV y colisiones.** `VehicleBody` (cuerpo rígido, 4 muelles, neumáticos, esquinas elásticas, autoenderezado), ruedas separadas del modelo que suben y bajan, centro de masas bajo las ruedas. `CollisionShape` (`Capsule`, `Box`), forma por defecto en todo `GameObject`, rejilla fija en `Stage` y `contactFloor`/`applyCollision`/`mass` en `DynamicGameObject`. El RV es el jugador del mapa de día. Verificado con programas de prueba sin ventana (en el directorio temporal, no se versionan): 57 049 choques aleatorios entre formas se separan bien; ninguna caja queda bajo el suelo en cinco posturas (también boca abajo) y en los dos modos de suelo; el RV sube rampas de 15/20/25° y vuelve a las ruedas desde cualquier postura en 0.7–2 s; el stage real no tiene solapes. Lecciones: la corrección dura del stage no tiene rozamiento ni par (si actúa antes que los contactos elásticos, el RV "se desliza en cámara lenta"); un contacto sin tope de fuerza dispara el chasis si nace dentro del suelo; el rayo del suelo no debe descartar puntos muy hundidos (el corte es la altura del objeto).
- 2026-10-04 (`main`): documentación al día: README, ARCHITECTURE.md (secciones "Suelo y colisiones" y "El RV") y **docs/UML.md** (10 diagramas Mermaid: vista general, mundo, física, render, entrada, audio y diálogos, UI, y secuencias de un frame, de la física y de una conversación, más el índice de las 71 clases). Todos validados con `mermaid.parse` y dibujados con mermaid-cli.

## Próximos pasos / ideas

(Rellenar según lo que se decida con el usuario.)
- Una voz mejor: instalar `mbrola-es1..4` (sudo apt) y usar `mb-es2`, o una TTS neuronal (Piper TTS en un venv + modelo es_ES) como otra subclase de `SpeechSynthesizer`.
- Volumen en Opciones (`SoundEngine::setMasterVolume`), NPCs en `.scene` (comando `npc`), subtítulos palabra a palabra.
- ¿Pausar el mundo con el menú abierto? Pantalla de Audio (volumen) como otro `SettingsMenu`.
- ¿Pasar también el mapa de día a `.scene` (para verlo en el visor)? Habría que respetar que es código de `main`.
- Entrar y salir de la autocaravana (que sea `Interactable` y cambie el `PlayableCharacter` del `Controller`; hoy el RV es el jugador del día y el pingüino a pie se queda quieto). Un botón para recolocar un RV volcado. Ruedas con giro visible (hoy solo suben, bajan y dirigen: son discos sin textura).
- Modelo del pingüino apoyado por los pies (hoy `AnimatedModel` lo centra en su posición); solo afecta a la tercera persona.
- Posibles: rueda del ratón o teclado en la UI, satélite en el `.scene` y en el visor, delta time, que el jugador siga la altura del terreno, varias animaciones (andar/parado) usando `Animation`, una clase Renderer.
