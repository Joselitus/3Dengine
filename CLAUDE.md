# CLAUDE.md

Memoria de trabajo del proyecto. **Léela al empezar cada sesión y actualízala al terminar** cualquier cambio relevante (arquitectura, convenciones, decisiones, tareas pendientes). Debe ser concisa: aquí va lo que no se deduce rápido leyendo el código. El detalle técnico está en [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), que también hay que mantener al día.

## Qué es

Juego 3D en C++ sobre un motor propio (OpenGL 3.3 core). El objetivo es construir el motor y su arquitectura a la vez que el juego. Dos mapas, que se cambian con el selector de depuración (tecla Z):
- **"Desierto de dia"** (`TestStage`, en código en `test.cpp`): dunas, carretera, cactus, rocas, una autocaravana aparcada y un satélite orientable.
- **"Desierto de noche"** (`SceneStage` + `assets/scenes/desert.scene`): cielo estrellado, luna y la criatura.

En los dos, el jugador es un pingüino a pie, en primera persona a la altura de la cabeza.

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

**Física y colisiones.** `RV` es un `VehicleBody` (chasis rígido sobre 4 muelles, ruedas separadas en `wheel_negx/posx.obj`; ver `VehicleBody.h`), con el peso bajo (centro de masas bajo las ruedas) y un par de autoenderezado tipo tentetieso: tiende siempre a volver a apoyarse en las ruedas). Cada `GameObject` tiene una forma de colisión (`CollisionShape.h`): por defecto una cápsula ajustada al modelo, o la que se pase al construir (el RV usa una `Box`). `Stage` tiene una rejilla fija de celdas (8 m) y solo prueba pares que comparten celda; los objetos no se solapan y la forma de un dinámico no atraviesa el suelo. El suelo y la carretera deben ser `setCollidable(false)`. Los estáticos se registran en la rejilla al hacer `add()`: no moverlos después.

## Arquitectura (resumen)

**`main` manda** (es el código de la otra persona). Lo nuestro se construye heredando de sus clases, no al revés.

- **Jerarquía de objetos:** `GameObject` (piezas `Part` compartidas o un `AnimatedModel`; posición, rotación, escala; `update(dt)` virtual; `setVisible`). De él deriva `DynamicGameObject` (velocidad, gravedad, `steerTowards`), y de este `PlayableCharacter` (abstracta: `control`, `attachCamera`, `followCamera`), que implementan `Walker` (el jugador) y `RV`.
- **`Stage`** (abstracta, de `main`) es dueño de los objetos (`add`, `addDynamic`) y del suelo (`setFloor`, `floorAt`, `collideWithFloor`).
- **`GameStage : Stage`** (abstracta, nuestra) es un mapa jugable: `Environment` (luz y horizonte), cielo opcional, jugador y cámara, interactuables y `render()`. De ella derivan `TestStage` (día, en código) y `SceneStage` (cualquier `.scene`).
- **Mapas:** la lista `maps` de `main()` (nombre + fábrica). Cambiar de mapa siempre pasa por `switchMap`, **diferido** al principio del frame (`requestedMap`), nunca dentro de un callback de la UI. `switchMap` cierra los paneles, vacía el `InteractionSystem`, cambia el stage, conecta el `Controller` y aplica el entorno. Nada puede guardar punteros a objetos del mapa sin limpiarlos ahí. Guía "Añadir un mapa" en ARCHITECTURE.md.
- **Cámara:** `Controller` → `PlayableCharacter::attachCamera(camera, distancia, altura)`. La distancia 0 es primera persona: `Walker` se oculta. Hoy es `attach(player, 0, 1.6)`, con la posición del jugador en sus pies.
- **`.scene`**: mapas en datos. Los leen `SceneFile` → `SceneStage` y el visor web, cuyos parsers deben estar sincronizados. `floor` define el suelo, y una `y` = `ground` apoya el objeto en él; úsalo siempre, porque las dunas se regeneran. La clase `Scene` antigua se eliminó.
- Un solo shader para el mundo: `animatedshader.vert` + `shader.frag`. **Trampa:** en el shader, `model` es la *rotación de la cámara*, no la matriz del objeto. El objeto usa `objposition` + `objrotation` (que incluye la escala).
- Uniforms de control: `skinned`, `unlit` (0 iluminado, 1 cielo, 2 emisivo; por pieza), `breathAmp`/`breathTime`, `fitCenter`/`fitScale`. Tabla completa en ARCHITECTURE.md.
- **Interfaz 2D propia** (`UI*`):
  - `UIElement` es la base y de ella derivan `UILabel`, `UIButton`, `UISlider` y `UIContainer` (de la que salen `UIPanel` y `UIRow`).
  - `UIManager` gestiona los paneles y el ratón. `UIRenderer` usa su propio shader y deja activo de nuevo el del motor al terminar.
  - El texto usa `stb_easy_font`, que solo tiene ASCII: `UIRenderer::toAscii` quita las tildes al dibujar. Escribe los textos en español correcto (la voz los necesita así); en pantalla saldrán sin tildes.
- **Objetos usables:** heredan de `Interactable` y rellenan su panel en `buildInterface`. `TestStage::getInteractables()` los registra en el `InteractionSystem`, que muestra el aviso "E: usar …" y abre o cierra su panel con E.
- **Teclado y Esc (importante):** `UIManager` es **el único dueño** del *key callback* de GLFW y del *user pointer* de la ventana; no instales otros. Las teclas van al `onKey` del panel de arriba; Esc cierra ese panel si no la gestiona, y sin paneles se ejecuta su atajo (`ui.bindKey`): Esc → `PauseMenu`, Z → `MapSelector`. `Controller` ya no lee Esc.
- **Pausa de controles:** una sola regla en el bucle de `test.cpp`: `controller.setEnabled(!ui.hasPanels())`. No pauses el `Controller` desde paneles ni sistemas.
- **Teclas:** `Controls` es la única fuente (`Action` → tecla). **No escribas `GLFW_KEY_...` para una función del juego:** añade una `Action` y léela con `controls.key(...)`, para que salga en la pantalla de controles y se pueda reasignar. Solo Esc (el "atrás" de la UI) es fijo. Los pasos para la futura reasignación están en ARCHITECTURE.md, sección "Controles y teclas"; ojo con los atajos de `bindKey`, que se registran por tecla.
- **Configuración guardada:** `Settings` (`clave = valor` en `~/.config/3dengine/settings.cfg`, o `$XDG_CONFIG_HOME`). `OptionsMenu` la aplica al arrancar (`applySettings`) y la guarda al cerrarse (destructor). **En las pruebas usa `XDG_CONFIG_HOME=<temporal>`** para no pisar la configuración del usuario.
- **Menús:** `PauseMenu` (Reanudar / Opciones / Salir, la tecla X sale), `OptionsMenu` → `ControlsMenu` (lista informativa de teclas, generada desde `Controls`), `MapSelector` (Z, debug). Todos reciben un `MenuContext` (ui, camera, controls, settings, quit). y `OptionsMenu` (sensibilidad y FOV de la `Camera`, aplicados al momento y no guardados en disco) son **subclases de `UIPanel`**. Para cualquier ventana nueva, sigue la guía "Crear una ventana nueva" de ARCHITECTURE.md: heredar de `UIPanel`, añadir hijos en el constructor, `onKey` para atajos, `closable = false` y `dimsBackground()` en menús, y `ui.open(...)` + `requestClose()` para pasar de una ventana a otra.
- **Cámara:** `Camera::setAngles(yaw, pitch)` en radianes, más `setFov`/`setSensitivity`. `Controller` acumula el giro a partir del desplazamiento del ratón en cada frame (antes usaba la posición absoluta del cursor, y cambiar la sensibilidad hacía saltar la cámara).
- **Audio y voz:** `SoundEngine` (miniaudio, cabecera única en `src/`) con `Sound`/`AudioClip`; el oyente sigue a la cámara cada frame. `SpeechSynthesizer` es abstracta y `EspeakSynthesizer` lanza `espeak-ng` con fork/exec, así que **espeak-ng es una dependencia en tiempo de ejecución**. `Voice` sintetiza con `std::async` y da `progress()` para los subtítulos (`UITextBlock`). `Npc : DynamicGameObject, Interactable` (de momento "Pingu" en `TestStage`).
- **Diálogos:** `Dialogue` es la caja común (páginas, "Siguiente"/"Cerrar", Esc) y usa un `LineNarrator`: `Voice` (TTS) en `Npc`, `Typewriter` (silencioso, letra a letra) en `Readable : GameObject, Interactable` (el "Cartel" de `TestStage`, `assets/sign/`). Para un objeto que se lee, usa `Readable`; para otra forma de entregar el texto, crea una subclase de `LineNarrator`. El narrador se declara antes que el `Dialogue`. `Interactable::getInteractionVerb()` da el verbo del aviso ("hablar con", "leer"). `Interactable` tiene ahora `onInterfaceOpened`/`onInterfaceClosed`. `AnimatedModel(path, feetAtOrigin)`. Todo está en la sección "Audio y voz" de ARCHITECTURE.md.
- **Para probar el audio:** usa siempre una salida virtual (`pactl load-module module-null-sink sink_name=engine_test`, juego con `PULSE_SINK=engine_test`, `parec --device=engine_test.monitor`) y descárgala al terminar. Grabar el monitor por defecto mezcla lo que el usuario escucha (le pasó: tenía audio sonando), y además así el usuario no oye las pruebas.
- `Satellite`: la cabeza es el objeto y el poste un `GameObject` aparte (`getMount()`), porque todas las piezas de un `GameObject` comparten la misma transformación. Azimut desde −z en sentido horario hacia +x; cénit 0 = vertical. Gira hacia el objetivo a `slewRate` °/s.

## Puntos que deben estar sincronizados

- `SceneFile::load` ↔ `parseScene` en `viewer.js`.
- `animatedshader.vert`/`shader.frag` ↔ los shaders copiados en `viewer.js`.
- `moon` del `.scene` ↔ `MOON_DIR` en `assets/sky/generate_sky.py`. `fog` ↔ `HORIZON`.
- Altura `y` de los objetos del desierto = `-1 + dune_height(x, z) - 0.05` (`assets/desert/generate_assets.py`).
- Constantes del visor (`FOV`, `SENSIVILITY`, `PLAYER_HEIGHT`, `BREATH_AMPLITUDE`) ↔ `Camera`, `Controller`, `AnimatedModel`, `SceneStage`.

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

- 2026-10-04 (`main`): **menú de pausa y de opciones.** Esc → `PauseMenu` (Opciones, Salir, tecla X). `OptionsMenu` ajusta la sensibilidad (0.2–3x) y el FOV (40–110°). Cambios en la UI genérica: `onKey`, *key callback* en `UIManager`, Esc genérico con *escape handler*, `open(UIPanel*)`/`close`/`isOpen`, y `UIPanel` con `closable` y `dimsBackground`. `Camera` gana FOV y sensibilidad ajustables y ángulos en radianes (`setAngles`; se quitan `rotate(int,int)`, `getPhi` y `getTheta`). `Controller` pasa a usar el desplazamiento del ratón y deja de leer Esc. `InteractionSystem` ya no conoce el `Controller`. Verificado en Xephyr: pausa, opciones, FOV 90, sensibilidad 2x (60 px → 0.6 rad), Esc entre menús, satélite, salir con X y con el botón.

- 2026-10-04 (`main`): botón **Reanudar** en la pausa. **Selector de mapas de depuración** (Z) y mapa de noche recuperado. Se añaden `GameStage` (entre `Stage` y los mapas) y `SceneStage` (carga `.scene`); `TestStage` pasa a heredar de `GameStage` y se le mueve el entorno que estaba en `main()`. Formato `.scene`: `floor` y `y` = `ground`; `desert.scene` actualizado (cámara `0 1.6`, objetos `ground`); el visor entiende `floor`/`ground` (con un raycast hacia abajo). `UIManager::bindKey` sustituye a `setEscapeHandler`. `Light::setColor`, `InteractionSystem::clear` y `Controller::attach` reinician la vista. Se elimina la clase `Scene`. Verificado en Xephyr: Reanudar, Z, noche (cielo, criatura, andar), Z/Z, vuelta al día con el satélite, X; y el visor.

- 2026-10-04 (`main`): **pantalla de Controles** (Opciones → Controles), solo informativa. Registro central `Controls` (`Action` → tecla, `describe`, `group`, `keyName`, `fixedControls`); `Controller`, `InteractionSystem`, `PauseMenu`, `MapSelector` y `test.cpp` leen de él y ya no queda ninguna tecla del juego escrita a mano (solo Esc, que es fijo). Se añaden `MenuContext` (los menús reciben ui/camera/controls/quit) y `UIInfoRow`. El texto del panel del satélite pasa a "Esc: cerrar". Verificado en Xephyr: Esc → Opciones → Controles → Esc×3, W, Z/Z, X. Confirmado además que todo lo nuestro hereda de las clases de `main` sin modificarlas (`Stage`, `DynamicGameObject`, `PlayableCharacter`, `RV`).

- 2026-10-04 (`main`): **TTS + motor de sonido + NPC.** miniaudio 0.11.21 (`miniaudio.h`/`.cpp`, compilado con `-w`; makefile con `-pthread -ldl`). `SoundEngine`/`Sound`/`AudioClip`, `SpeechSynthesizer` → `EspeakSynthesizer` (voz `es`; las MBROLA no están instaladas; `/usr/bin/piper` es una aplicación de ratones, no TTS), `Voice` asíncrona, `Npc` "Pingu" en el mapa de día, `UITextBlock` (subtítulos que avanzan con la voz) y `UIRenderer::toAscii`. Verificado: el sintetizador funciona con tildes, comillas y `$(...)`; la grabación en una salida virtual muestra silencio → voz al pulsar E → fin de la frase → "Siguiente" → silencio inmediato con Esc. Subtítulos y NPC verificados en Xephyr.

- 2026-10-04 (`main`): diálogo del `Npc` sin "Repetir"; "Siguiente" pasa a "Cerrar" en la última frase y cierra el panel; cada conversación empieza en la frase 1; Esc corta la voz en cualquier momento. `UIButton` admite texto dinámico. Verificado en Xephyr con la salida virtual.

- 2026-10-04 (`main`): **sistema de lectura sin voz.** La lógica del diálogo sale de `Npc` a `Dialogue` + `LineNarrator` (`Voice` lo implementa; nuevo `Typewriter`). Se añaden `Readable` y un cartel en el mapa de día (`assets/sign/generate_sign.py`), y `getInteractionVerb`. Verificado en Xephyr con la salida virtual: el cartel se escribe solo, en silencio total, y sus tres páginas acaban en "Cerrar"; Pingu sigue hablando y Esc lo corta; los avisos dicen "E: hablar con Pingu" y "E: leer Cartel".

- 2026-10-04 (`main`): **las opciones se guardan entre sesiones.** Nueva clase `Settings` (fichero `clave = valor` en la carpeta de configuración del usuario, escritura atómica, conserva claves desconocidas). `OptionsMenu` aplica los valores al arrancar y guarda al cerrarse; `MenuContext` incluye `settings`. Verificado con una prueba unitaria de `Settings` (fichero nuevo, releer, líneas rotas) y con dos sesiones en Xephyr usando un `XDG_CONFIG_HOME` temporal: el FOV 90 y la sensibilidad 2x se conservan.

## Próximos pasos / ideas

(Rellenar según lo que se decida con el usuario.)
- Una voz mejor: instalar `mbrola-es1..4` (sudo apt) y usar `mb-es2`, o una TTS neuronal (Piper TTS en un venv + modelo es_ES) como otra subclase de `SpeechSynthesizer`.
- Volumen en Opciones (`SoundEngine::setMasterVolume`), NPCs en `.scene` (comando `npc`), subtítulos palabra a palabra.
- Reasignar teclas (pasos en ARCHITECTURE.md, "Controles y teclas"); hace falta `UIManager::unbindKey`.
- ¿Pausar el mundo con el menú abierto? Guardar también las teclas cuando se puedan reasignar (en `Settings`).
- ¿Pasar también el mapa de día a `.scene` (para verlo en el visor)? Habría que respetar que es código de `main`.
- Entrar en la autocaravana (que sea `Interactable` y cambie el `PlayableCharacter` del `Controller`).
- Modelo del pingüino apoyado por los pies (hoy `AnimatedModel` lo centra en su posición); solo afecta a la tercera persona.
- Posibles: rueda del ratón o teclado en la UI, satélite en el `.scene` y en el visor, delta time, que el jugador siga la altura del terreno, varias animaciones (andar/parado) usando `Animation`, una clase Renderer.
