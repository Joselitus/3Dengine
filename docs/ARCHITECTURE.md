# Arquitectura del motor

Este documento explica cómo está montado el motor 3D y cómo se dibuja un frame. También indica qué hay que tocar para las tareas más habituales. Para compilar y ejecutar, consulta el [README](../README.md). Para ver cómo encajan las clases, mira los [diagramas UML](UML.md).

## Mapa del repositorio

```
src/                 motor + juego (C++11, OpenGL 3.3 core)
  test.cpp           main: lista de mapas, cambio de mapa, TestStage (el mapa de día) y bucle principal
  render/            mallas, modelos, animación, Shader, Light, Camera, LineRenderer (líneas de depuración), myopengl
  effects/           ParticleEmitter (simulación) y ParticleRenderer (dibujo) de las partículas
  physics/           CollisionShape (cápsula, caja) y VehicleBody (chasis sobre muelles)
  world/             GameObject, DynamicGameObject, PlayableCharacter, Stage, GameStage, SceneStage, SceneFile, Interactable
  entities/          RV, Walker, Npc, Satellite, Readable
  input/             Controller, Controls, InteractionSystem
  debug/             DebugSelector (modo selección de objetos, tecla 1)
  audio/             SoundEngine, AudioClip, Voice, SpeechSynthesizer, EspeakSynthesizer
  dialogue/          Dialogue, LineNarrator, Typewriter
  ui/                UIManager, UIRenderer, UIOverlay, los UI* y los menús (Pause, Options, Controls, Settings, Camera, ConfirmDialog, MapSelector)
  core/              Settings (ajustes entre sesiones), TextFormat.h (printf a std::string, vectores en texto)
  shaders/           animatedshader.vert + shader.frag (el programa del mundo), ui.vert/ui.frag, particle.vert/particle.frag, lines.vert/lines.frag; shader.vert está en desuso
  third_party/       miniaudio, stb_image, stb_easy_font (se compilan con -w)
  makefile           compila todos los .cpp de src/ (en cualquier subdirectorio) y genera ../test/test.
                     Cada subdirectorio es un include path: los .h se incluyen por su nombre, sin ruta
assets/
  scenes/*.scene     mapas en datos (SceneStage), también los muestra el visor
  desert/ sky/ creature/ rv/ cube/   assets procedurales + su script generate_*.py
  ping/              pingüino animado (FBX), el jugador
  backpack/          modelo de ejemplo (sin usar)
tools/scene_viewer/  visor web de escenas (ver su README)
docs/                ARCHITECTURE.md (esto) y UML.md (diagramas de clases y secuencias)
```

## Módulos

```
                     test.cpp (main: mapas y bucle)
      ┌────────────┬──────┴───────┬──────────────┬──────────┐
  Controller   GameStage (actual)  UIManager  InteractionSystem  Light, Shader
      │             │                 │              │
    Camera          │                UI*        Interactable
                    │
   Stage (abstracta, de main) ── GameStage (abstracta: entorno, cielo, jugador, interactuables)
                                   ├── TestStage   (test.cpp, desierto de día; en código)
                                   └── SceneStage  (un .scene; desierto de noche)

   GameObject ─ Model, CollisionShape (Capsule | Box)
                    ├── Satellite, Readable (+ Interactable)
                    └── DynamicGameObject
                          ├── Npc (+ Interactable)
                          └── PlayableCharacter (abstracta)
                                ├── Walker  (a pie, 1ª persona: el jugador de la noche)
                                └── RV      (autocaravana con VehicleBody; se sube por su puerta, + Interactable)
           (todo usa myopengl: utilidades GL, texturas, conversión de matrices)
```

| Clase | Responsabilidad |
|---|---|
| `Shader` | Compila y enlaza vert+frag desde ficheros y expone setters de uniforms. El del motor queda activo tras crearse. |
| `Mesh` | Geometría estática en GPU (VAO/VBO/EBO) y sus texturas. |
| `Model` | Carga un fichero con Assimp y crea un `Mesh` por cada `aiMesh`. Ignora las transformaciones de los nodos. |
| `AnimatedMesh` | Como `Mesh`, pero con 12 pares (hueso, peso) por vértice. |
| `AnimatedModel` | Carga un FBX con esqueleto: mallas, lista global de huesos, `Skeleton` y ajuste de escala (`computeFit`). Posee el `Assimp::Importer`. |
| `Skeleton` | Evalúa la primera animación del fichero y calcula las matrices de hueso (`boneMats`, como máximo 100). |
| `GameObject` | Todo lo que está en el mundo. Hecho de piezas (`Part`: un `shared_ptr<Model>`, su modo `unlit` y una transformación local opcional, que usan las ruedas del RV) o de un `AnimatedModel`, con posición, rotación y escala. Tiene una **forma de colisión** (`CollisionShape`; por defecto una cápsula ajustada al modelo, o la que se pase al construir) y un flag `collidable`. `update(dt)` es virtual. `Draw()` fija los uniforms y no dibuja nada si `setVisible(false)`. |
| `DynamicGameObject` | `GameObject` que se mueve: velocidad, aceleración, velocidad máxima, masa, gravedad y **rozamiento** (`setDrag`, en 1/s: cómo muere la velocidad horizontal; 0 por defecto, o sea, nunca). Quien lo controla lo guía con `steerTowards` (aceleración); el `Stage` lo mueve. **Varias mallas:** el modelo con el que se construye es la malla 0 y `addMesh(modelo)` (estático o animado) añade más, todas cargadas desde el principio; `setMesh(i)` cambia cuál se dibuja (un índice desconocido se ignora, y las demás piezas, como las ruedas, no se tocan). Solo se actualiza la malla visible (`GameObject::update` mueve `aniModel`), así que una animación que no se ve no corre. Ganchos virtuales para el stage: `contactFloor` (el objeto lleva su propio suelo, como el RV) y `applyCollision` (cómo recibe un empujón). |
| `PlayableCharacter` | Personaje que maneja el `Controller` (abstracta). Cada uno decide cómo responde a la entrada (`control`) y cómo lo sigue la cámara (`attachCamera`, `followCamera`). |
| `Walker` | Personaje a pie: anda en la dirección de la cámara y se orienta hacia donde camina. Con distancia de cámara 0 es primera persona y se oculta a sí mismo. Es el jugador de los dos mapas (en el de día puede subir al RV). |
| `RV` | Autocaravana del mapa de día (también `Interactable`: se sube por su puerta): W/S aceleran y A/D giran las ruedas delanteras, y la cámara orbita libremente sin girar la malla. Es un `VehicleBody` (chasis sobre 4 muelles) con las ruedas como piezas aparte. Ver [El RV](#el-rv-vehículo-con-suspensión). Su forma de colisión es una caja larga. Cada rueda tiene un emisor de **polvo** que echa en la arena. Ver [Partículas](#partículas). |
| `Satellite` | `GameObject` + `Interactable`: cubo orientable en azimut y cénit. Ver [Satélite](#satélite). |
| `Stage` | Nivel (abstracta): es dueño de los objetos estáticos y dinámicos, carga cada modelo una sola vez, tiene el suelo (height field o rayo hacia abajo, `floorAt`) y la **rejilla de colisiones**. Cada frame actualiza los objetos, aplica `apply()` a los dinámicos y resuelve las colisiones. Ver [Suelo y colisiones](#suelo-y-colisiones). Tiene la **música de fondo** del nivel (`setMusic`/`loadMusic`, null = sin música; en bucle por defecto). Ver [Música de fondo](#música-de-fondo). También dice **de qué es el suelo** en cada punto (`materialAt`), a partir del mapa de materiales que se le da con el suelo. Ver [Materiales del suelo](#materiales-del-suelo). Tiene los **emisores de partículas** del nivel (`addEmitter`), que mueve cada frame. |
| `FloorMaterial`, `MaterialMap` | De qué es el suelo (arena, asfalto...) y el mapa que lo dice punto a punto (una imagen de números de material sobre el suelo, o uno uniforme). Un suelo de tipo `HeightField` lo necesita. Ver [Materiales del suelo](#materiales-del-suelo). |
| `ParticleEmitter`, `ParticleRenderer` | Partículas muy básicas: el emisor las simula (nacen a un ritmo, en un cono, con gravedad y rozamiento, y se desvanecen) y el renderizador las dibuja como discos que miran a la cámara. Ver [Partículas](#partículas). |
| `CollisionShape`, `Capsule`, `Box` | Volumen de colisión de un objeto (abstracta + pastilla vertical + caja orientada), con la prueba de choque entre cualquier par (`collide`) y los puntos bajos que no pueden quedar bajo el suelo (`floorSamples`). Ver [Suelo y colisiones](#suelo-y-colisiones). |
| `VehicleBody` | Física de un vehículo con ruedas, sin OpenGL: cuerpo rígido con masa e inercia sobre muelles amortiguados (un "vehículo de rayos"), neumáticos y autoenderezado. Lo usa `RV`. Ver [El RV](#el-rv-vehículo-con-suspensión). |
| `GameStage` | Mapa jugable (abstracta, hereda de `Stage`): añade todo lo que el juego necesita para ejecutarlo y cambiarlo en marcha. Incluye el `Environment` (dirección y color de la luz, color del horizonte), el cielo opcional (`setSky`) y el jugador con la cámara que quiere (distancia, altura). También tiene los interactuables, una regla `apply()` por defecto (suelo) y `render()` (cielo alrededor de la cámara + stage). Puede **cambiar de jugador** en marcha: `setPlayer(personaje, distancia, altura, yaw)` marca el cambio y el bucle principal lo recoge con `takePlayerChange()` y vuelve a conectar el `Controller`. `leaveVehicle()` (tecla de bajar) y `interactionsEnabled()` son ganchos virtuales para los mapas con vehículos. |
| `TestStage` (`test.cpp`) | Mapa "Desierto de dia", montado en código: dunas (el suelo, `dunes_loop.obj`, de 180 × 180 m), una **carretera de 8 m de ancho que forma un circuito cerrado de unos 250 m** alrededor del claro de salida (`road.obj`), 44 cactus y rocas repartidos fuera de la carretera, el satélite, un cartel, un pingüino a pie (`Walker`) y un NPC (Pingu), con luz de sol. **El jugador empieza siendo el pingüino a pie, en primera persona**; al usar la puerta del `RV` pasa a conducirlo, en tercera persona (cámara a 12 de distancia y 3.5 de altura), y con Mayús vuelve a pie. Ver [Subir y bajar del RV](#subir-y-bajar-del-rv). El suelo y la carretera no son colisionables. |
| `SceneStage` | Mapa a partir de un `.scene` ("Desierto de noche" = `desert.scene`): suelo, objetos (apoyados con `ground`), efectos, cielo, luz y un `Walker` como jugador. |
| `MapSelector` | Menú de depuración (tecla Z) para cambiar de mapa (subclase de `UIPanel`). Ver [Mapas](#mapas-y-selector-de-depuración). |
| `Camera` | Calcula las matrices de proyección y vista y sigue a un `GameObject`: en primera persona (distancia 0, a la altura de los ojos) o en tercera, desde detrás. El FOV (`setFov`) y la sensibilidad (`setSensitivity`) se pueden cambiar en marcha. |
| `Controller` | Gestiona la entrada: el desplazamiento del ratón en cada frame × la sensibilidad gira la cámara, y las teclas de movimiento (WASD, ver `Controls`) llegan al `PlayableCharacter`. Se puede pausar (`setEnabled(false)`), y entonces el personaje recibe una entrada nula. **No lee Esc.** |
| `PauseMenu`, `OptionsMenu`, `SettingsMenu`, `CameraMenu`, `ControlsMenu`, `AudioMenu` | Menús del juego (subclases de `UIPanel`) que reciben un `MenuContext`. `SettingsMenu` es la base de las pantallas de ajustes (Guardar/Salir con aviso). Ver [Menús](#menús-pausa-y-opciones). |
| `Controls` | Registro de teclas: qué tecla hace cada `Action`. Todo lo que lee teclado lo consulta aquí. Ver [Controles](#controles-y-teclas). |
| `Interactable` | Interfaz (clase abstracta) de los objetos que el jugador puede usar: nombre, verbo, punto, alcance, `buildInterface(UIPanel&)` y ganchos. Por defecto usarlo abre un panel; si `usesDirectly()` devuelve `true` (el RV), `onUse()` actúa al momento y no se abre ningún panel. `isInteractionAvailable()` lo oculta mientras no se puede usar (un RV ya ocupado). |
| `InteractionSystem` | Busca el `Interactable` disponible más cercano al jugador, muestra el aviso ("E: <verbo> <nombre>") y, con E, abre o cierra su panel o, si se usa directamente, llama a `onUse`. Solo abre si no hay otro panel abierto. `update(pos, enabled)` con `enabled = false` (mientras se conduce) no ofrece ni usa nada. Esc lo cierra `UIManager`. |
| `UIManager`, `UIRenderer`, `UI*` | Sistema de interfaz 2D genérico. Ver [Interfaz de usuario](#interfaz-de-usuario-ui). |
| `Light` | La única luz puntual del shader (el sol, o la luna en el mapa de noche). |
| `SoundEngine`, `Sound`, `AudioClip` | Motor de sonido (miniaudio): reproduce clips en memoria, en 3D o directos; el oyente sigue a la cámara. Un `Sound` puede repetirse en bucle (`setLooping`). Ver [Audio y voz](#audio-y-voz-tts). |
| `SpeechSynthesizer`, `EspeakSynthesizer`, `Voice` | Texto a voz: la interfaz abstracta, su implementación con espeak-ng, y una voz que dice textos en segundo plano e informa del progreso. |
| `MusicPlayer` | Reproduce la música de fondo del mapa actual (una pista cada vez, directa a los dos oídos, no en un punto del mundo). `play(clip, bucle, volumen)` sustituye la pista anterior; `nullptr` la para. Ver [Música de fondo](#música-de-fondo). |
| `Npc` | `DynamicGameObject` + `Interactable` con un rozamiento grande (`NPC_DRAG` = 10/s), para que un empujón (el RV, el jugador) no lo haga deslizarse sin fin: se para en una décima de segundo. Personaje con un `Dialogue` que dice su `Voice`. Al usarlo se gira hacia el jugador y habla, con subtítulos sincronizados. Declara `onInteraction(const Interaction &)` (vacío por defecto), que se ejecuta tras cada interacción con el tipo (`Started` al abrirse el diálogo, `Finished` al cerrarse, de la forma que sea) y la posición del jugador; una subclase lo sobrescribe para reaccionar. |
| `FollaCulos` | `Npc` (sin diálogo) que es la criatura nocturna (`assets/folla_culos/folla_culos_run.glb`): **de noche corre en línea recta hacia el personaje que controla el jugador** (`setTarget`, una función que `TestStage` define como la posición de `player`, así que sigue al RV si se conduce) y se para a 1.3 m, y **de día huye de él** en línea recta hasta estar a 90 m (`FLEE_DISTANCE`); es de noche cuando el sol está bajo el horizonte (`setNightQuery`, que `TestStage` define con `environment.sunDir.y < 0`). Corre a `RUN_SPEED` = 4.34 m/s —lo que la animación recorre por zancada, para que los pies no patinen—. La animación solo avanza mientras corre. Sus ojos son un material emisivo amarillo y además da una luz omnidireccional amarilla de poco alcance (`getLight`, `EYE_LIGHT_RANGE` = 2.5 m, la octava del shader). Usa el modelo a su tamaño real (`AnimatedModel::useRealSize`) y colores planos (`AnimatedMesh::setColor/setEmissive`). |
| `Pingu` | `Npc` con dos mallas (`Dancing` = 0 y `Standing` = 1): baila por defecto y, mientras habla con el jugador, está de pie respirando (modelo en pose idle); su `onInteraction` es el cambiador de animación. `TestStage` carga el FBX dos veces (con `PENGUIN_ANIMATION`, uno con `setIdle(true)`). |
| `Dialogue`, `LineNarrator`, `Typewriter` | Caja de diálogo común (páginas, "Siguiente"/"Cerrar", Esc) y cómo se entrega cada línea: con voz (`Voice`) o escribiéndose en silencio (`Typewriter`). Ver [Diálogos](#diálogos-hablar-y-leer). |
| `Readable` | `GameObject` + `Interactable`: algo que se lee (un cartel). Usa la misma caja de diálogo, pero sin voz. |
| `SceneFile` | Parser de `.scene`, sin OpenGL. Lo usa `SceneStage`. (La antigua clase `Scene` se eliminó: la sustituye `SceneStage`.) |
| `Animation` | Rango de frames con nombre. **Todavía no se usa.** |

## Arranque

`main` cambia el directorio de trabajo a `src/`, que localiza junto al ejecutable a partir de `/proc/self/exe` (`test/test` → `test/../src`). Por eso los shaders se abren como `shaders/animatedshader.vert` y los assets como `../assets/...`, se lance desde donde se lance. Si falta un shader, `fileToString` lo dice y termina. Antes devolvía una cadena vacía y el driver acababa fallando al enlazar con un error confuso (`must write to gl_Position`).

## Un frame

```
[cambio de mapa pendiente]   si el selector (o el arranque) pidió un mapa: switchMap() aquí, fuera de la UI
camera.resize()              viewport y proyección si cambia el framebuffer
interaction.update(pos, enabled)   aviso "E: usar ..."; E abre o cierra el panel del objeto cercano (o lo usa al momento, como el RV)
ui.update()                  ratón y teclas → paneles; Esc cierra el de arriba; sin paneles, Esc → pausa y Z → mapas
[cambio de jugador]          si stage->takePlayerChange(): controller.attach(nuevo jugador, distancia, altura, yaw)
controller.setEnabled(!ui.hasPanels())   con cualquier panel abierto, controles en pausa y cursor libre
controller.setLookEnabled(!selector.capturesMouse())   en el modo colocación, con el botón derecho el ratón gira el objeto y no la cámara
controller.update()          ratón → rotación de la cámara; teclas → player->control(dir, up, yaw)
stage->update(dt)            mueve los objetos, aplica el suelo y resuelve las colisiones (ver "Suelo y colisiones")
player->followCamera()       la cámara sigue al jugador ya movido
stage->keepAboveFloor(...)   la cámara no se hunde en el suelo (Camera::RADIUS)
selector.update(stage, cam, !ui.hasPanels())   modos selección (1: clic → rayo y elige) y colocación (2: clic → relocate); refresca los datos
glClear(horizonte del mapa)
stage->render(shader, camPos, t)   cielo del mapa (si tiene) y después cada GameObject
particles.draw(emisores)     las partículas del mapa (polvo...), tras el mundo: discos que miran a la cámara
selector.draw(camera)        la forma de colisión y la AABB del objeto elegido (si el modo está activo)
ui.draw()                    interfaz 2D encima de todo: overlays (cruz y datos del selector), paneles y aviso
glfwSwapBuffers / glfwPollEvents
```

`dt` son los segundos desde el frame anterior: el movimiento no depende de los FPS.

## Sistema de coordenadas y transformaciones

- Mano derecha, **+y arriba**. Al empezar, la cámara mira hacia **−z**. El claro del desierto está en y = −1 (`GROUND_Y`), y fuera de él la altura la da el suelo del `Stage`. La posición de un personaje está en sus pies, sobre el suelo: a pie la cámara queda 1.6 por encima (primera persona); con el RV en tercera persona, 3.5 por encima y a 12 de distancia. El origen del RV está en el suelo, entre las ruedas.
- La rotación positiva alrededor de +y (yaw) es antihoraria vista desde arriba (`glm::rotate`).
- En el vertex shader: `world = objrotation * fitted + objposition` y `gl_Position = projection * model * view * world`.
  - **`model` no es la matriz del objeto.** Es la rotación de la cámara (pitch·yaw), que se aplica *después* de `view` (la traslación a la posición de la cámara). Así la cámara gira sobre sí misma.
  - `objrotation` contiene la rotación y también la escala del objeto. Las normales se transforman con su `mat3` y se normalizan en el fragment shader.

## Contrato del shader

El motor dibuja todo con **`shaders/animatedshader.vert` + `shaders/shader.frag`**. `shader.vert` es antiguo y ya no se usa.

| Uniform | Lo escribe | Significado |
|---|---|---|
| `projection`, `view`, `model`, `viewPosition` | `Camera::update` | Cámara (ver arriba). |
| `objposition`, `objrotation` | `GameObject::Draw` | Transformación del objeto. |
| `skinned` | `GameObject` (0) / `AnimatedModel` (1) | Activa el skinning. |
| `gBones[100]`, `meshMat` | `AnimatedModel::Draw` | Matrices de hueso. `meshMat` se usa con mallas animadas que no tienen pesos (siguen a su nodo). |
| `fitCenter`, `fitScale` | `AnimatedModel::Draw` | Normaliza el modelo animado a ~1.8 de alto alrededor del origen. |
| `idlePose` | `AnimatedModel::Draw` | 1 = el modelo con huesos se dibuja en su pose de reposo (`AnimatedModel::setIdle`): el vertex shader (`idle()`) baja las aletas y hace respirar la malla en pose de bind. 0 = esqueleto normal. |
| `breathAmp` / `breathTime` | `GameObject::Draw` / `Stage::Draw` | Respiración procedural de mallas estáticas. 0 la desactiva. |
| `alpha` | `Mesh::Draw` | Opacidad de la malla (1 = sólida; el `d` del `.mtl`). Menos de 1 se mezcla con lo que hay detrás. |
| `unlit` | `GameObject::Draw` (por pieza) | 0 = iluminado (Phong + niebla), 1 = cúpula de cielo con textura (estrellas que titilan), 2 = emisivo, 3 = cielo procedural (degradado, sol, estrellas y dunas en el horizonte). |
| `lightPosition`, `lightColor` | `Light` | La luz del sol: `lightPosition` es solo una **dirección** hacia la luz (el shader hace `normalize(lightPosition)`, no depende de dónde esté el objeto; `test.cpp` manda `lightDir * 100`). |
| `spotCount`, `spotPosition[]`, `spotDirection[]`, `spotColor[]`, `spotParams[]` | `test.cpp` (`applySpotLights`) | Focos (hasta 8, `MAX_SPOTS`; `SpotLight::omni` hace uno sin cono, para una bombilla): lo que devuelve `GameStage::getSpotLights` (hoy los faros del RV). `spotParams` = (coseno del cono interior, del exterior, alcance). Se suman a la luz del sol solo en el modo iluminado; se envían cada frame. |
| `moonDir`, `fogColor`, `time` | `test.cpp` | Dirección del astro y color del horizonte, que es también el de la niebla (de 80 a 140 unidades; el plano lejano de la cámara está a 300). Se envían **cada frame** (`applyEnvironment`), porque cambian con la hora. |
| `skyZenith`, `sunDir`, `starAlpha` | `test.cpp` | Cielo procedural (`unlit` = 3): color del cénit, dirección al sol y visibilidad de las estrellas. |
| `texture_diffuse1` (…) | `Mesh` / `AnimatedMesh::Draw` | Texturas del material. |

Atributos: 0 posición, 1 normal, 2 uv, 3–8 tres grupos `ivec4` de ids de hueso + `vec4` de pesos.

**El visor web contiene una copia de estos shaders** (`tools/scene_viewer/viewer.js`). Si cambias la iluminación, la niebla, el cielo o la respiración, cambia también la copia.

## Interfaz de usuario (UI)

Es un sistema propio y orientado a objetos (sin dependencias externas), pensado para que cualquier objeto del juego ofrezca su propio panel.

```
UIElement (abstracta)            colocación (layout), dibujo (draw) y ratón
├── UILabel                      texto fijo o generado cada frame (valores en vivo)
├── UIButton                     acción al pulsar y soltar encima (texto fijo o leído cada frame)
├── UISlider                     número en [min, max]; lee y escribe a través de funciones
├── UIInfoRow                    texto a la izquierda y valor a la derecha (p. ej. acción y tecla); clicable si tiene acción
├── UITextBlock                  párrafo con ajuste de línea; puede mostrar solo una fracción (subtítulos)
└── UIContainer (abstracta)      posee a sus hijos
    ├── UIPanel                  ventana: título, botón de cerrar, arrastrable; hijos en vertical
    └── UIRow                    hijos en horizontal, repartiendo el ancho

UIManager    paneles abiertos, reparto del ratón y del teclado, Esc, atajos, aviso inferior, dibujo
UIOverlay    (interfaz) algo que se dibuja a pantalla completa sin ser un panel: no recibe entrada ni pausa los
             controles (UIManager::addOverlay/removeOverlay; se dibuja bajo los paneles). P. ej. DebugSelector
ConfirmDialog (UIPanel) pregunta con varios botones ("¿Guardar y salir?"); se abre encima del panel que pregunta
UIRenderer   rectángulos y texto en píxeles de ventana, en un solo draw call
Interactable contrato entre un objeto del juego y la interfaz
```

- **Coordenadas:** píxeles de ventana con (0, 0) arriba a la izquierda, las mismas que `glfwGetCursorPos`.
- **Ratón:** `UIManager::update` busca el elemento bajo el cursor (`elementAt`, recursivo). Si es interactivo, recibe `onPress`, luego `onDrag` mientras se mantiene el botón y por último `onRelease`. Si no lo es (etiquetas, filas), el clic va a su panel, que se encarga del arrastre por el título y del botón de cerrar. El panel pulsado pasa al frente.
- **Teclado:** `UIManager` instala el *key callback* de GLFW (y el *user pointer* de la ventana), así que **nada más puede instalarlos**. Si algo necesita teclas, que lea con `glfwGetKey` o que pase por `UIManager`. Cada pulsación va al `onKey(key)` del panel de arriba. Si no la gestiona (devuelve `false`) y es Esc, el panel se cierra. Con ningún panel abierto, la tecla ejecuta su **atajo** (`ui.bindKey(tecla, acción)`). En el juego, Esc abre la pausa y Z el selector de mapas.
- **Pausa de controles:** una sola regla en el bucle: `controller.setEnabled(!ui.hasPanels())`. Ningún panel ni sistema tiene que tocar el `Controller`.
- **Valores en vivo:** `UILabel` y `UISlider` no guardan el valor, lo leen cada frame con una `std::function`. Así siempre muestran el estado real del objeto, aunque cambie por otra vía (por ejemplo, mientras el satélite gira).
- **Dibujo:** `UIRenderer` usa su propio shader (`shaders/ui.vert`/`ui.frag`) y, al terminar, **vuelve a activar el programa anterior**, porque los setters de `Shader` suponen que el shader del motor está activo. Desactiva el depth test y activa el blending solo mientras dibuja.
- **Texto:** se dibuja con `stb_easy_font.h` (de dominio público y en `src/third_party/`), que solo tiene ASCII. `UIRenderer::text` acepta UTF-8 y lo pasa a ASCII con `toAscii`: á → a, ñ → n, ¿ y ¡ desaparecen, ° → " deg", y cualquier otro carácter → "?". Así se puede escribir español correcto (lo que necesita la voz) y en pantalla sale sin tildes.
- **Estilo:** los colores y márgenes son comunes y están en `UITheme` (`UIElement.h`).
- **Abrir paneles:** `ui.open(Interactable&)` coloca el panel a la derecha. `ui.open(new MiPanel(...))` abre cualquier panel (el `UIManager` pasa a ser su dueño) centrado. `ui.close(panel)` y `panel->requestClose()` lo cierran al final del `update`, así que se pueden llamar desde sus propios botones o teclas. `ui.isOpen(panel)` solo compara punteros y es seguro aunque el panel ya no exista.
- **Opciones de `UIPanel`:** el constructor es `UIPanel(título, ancho = 360, closable = true)`. Con `closable = false` no hay botón de cerrar (útil en menús, donde la X confundiría). Si `dimsBackground()` devuelve `true`, el juego se oscurece detrás.

**Hacer que un objeto se pueda usar:**
1. Hereda de `Interactable` e implementa `getInteractionName()`, `getInteractionPoint()` y `buildInterface(UIPanel&)` (opcionalmente también `getInteractionRange()`, que por defecto es 3).
2. En `buildInterface`, añade elementos con `panel.add(new UILabel(...))` y similares. Las lambdas pueden capturar `this`, siempre que el objeto viva más que el panel. Si tiene que reaccionar al abrirse o cerrarse el panel (por ejemplo, un NPC que empieza a hablar y se calla), sobrescribe `onInterfaceOpened(posiciónJugador)` y `onInterfaceClosed()`.
3. Créalo en el mapa (`GameStage`), por ejemplo en `TestStage`: `add(objeto)` e `interactables.push_back(objeto.get())`. Al cargar el mapa, `switchMap` registra todos los de `getInteractables()` en el `InteractionSystem`.

### Crear una ventana nueva (guía)

Para un menú o ventana reutilizable, **hereda de `UIPanel`**, como `PauseMenu` y `OptionsMenu` (o de `SettingsMenu` si es una pantalla de ajustes que se guarda):

```cpp
class MiMenu : public UIPanel {
public:
  MiMenu(UIManager &ui, Algo &algo) : UIPanel("Titulo", 300.0f, false) {
    add(new UILabel([&algo]() { return "Valor: " + algo.texto(); }));
    add(new UISlider("Nivel", 0, 10, 1, [&algo]() { return algo.nivel(); },
                     [&algo](float v) { algo.setNivel(v); }));
    add(new UIButton("Aceptar", [this]() { requestClose(); }));
  }
  bool dimsBackground() const override { return true; }   // si es un menú
  bool onKey(int key) override {                         // atajos de teclado
    if (key != GLFW_KEY_ENTER) return false;             // false: Esc cierra
    requestClose();
    return true;
  }
};
// en cualquier sitio con acceso al UIManager:  ui.open(new MiMenu(ui, algo));
```

Reglas para no romper nada:
- **El texto se muestra en ASCII:** se puede escribir con tildes y ñ, pero se verán sin ellas (`toAscii`). Otros símbolos salen como "?".
- Las lambdas capturan referencias o `this`: lo capturado **tiene que vivir más que el panel**. Si un valor depende de un objeto que puede desaparecer, cierra el panel antes.
- Para **cambiar de ventana** (ir de un menú a otro), abre la nueva con `ui.open(...)` y cierra la actual con `requestClose()`, como `PauseMenu` → `OptionsMenu` → `PauseMenu`.
- Un elemento de un tipo nuevo hereda de `UIElement`: implementa `preferredHeight()` y `draw()`, y, si reacciona al ratón, `isInteractive()` → `true` y `onPress`/`onDrag`/`onRelease`. Para un contenedor nuevo, hereda de `UIContainer` y reimplementa `layout()`.
- No guardes el valor en el elemento: léelo y escríbelo con funciones, como hace `UISlider`.
- Si la ventana es de un objeto del mundo, mejor `Interactable` que una subclase de `UIPanel` (ver arriba).

## Menús (pausa y opciones)

- **Esc durante el juego** abre `PauseMenu` ("Pausa"), con el atajo `ui.bindKey(GLFW_KEY_ESCAPE, ...)` de `test.cpp`. Tiene **Reanudar** (igual que Esc: cierra el menú), **Opciones** y **Salir**. El botón o la tecla de `Action::Quit` (X) llaman a `quit` (`glfwSetWindowShouldClose`).
- Los menús reciben un **`MenuContext`** (`UIManager`, `Camera`, `Controls`, `Settings`, `SoundEngine`, `quit`) y se lo pasan unos a otros al navegar (Pausa → Opciones → Controles y vuelta). Si un menú nuevo necesita algo más, añádelo a `MenuContext`, no a cada constructor.
- **`OptionsMenu`** ("Opciones") es solo una lista de pantallas de ajustes: **Cámara**, **Controles**, **Audio** y **Volver** (o Esc, que vuelve a la pausa).
- **`SettingsMenu`** (abstracta) es la base de esas pantallas. Una subclase añade sus controles y termina su constructor con `addFooter()`, que añade un texto de estado y los botones:
  - **Por defecto** llama a `resetToDefaults()` (hay que guardar después);
  - **Guardar** llama a `apply()`, que hace definitivos los cambios y escribe `Settings`;
  - **Volver** (o Esc) vuelve a Opciones. Si `hasUnsavedChanges()`, antes abre un `ConfirmDialog` con "Guardar y salir" y "Salir", que llama a `discard()`. Esc sobre el aviso vuelve a la pantalla.

  La subclase implementa esos cuatro métodos y, si quiere, `hint()`.
- **`CameraMenu`** ("Cámara") tiene la **sensibilidad** (0.2x–3x, múltiplos de `SENSIVILITY` = 0.005 rad/píxel) y el **FOV** vertical (40–110°, por defecto `DEFAULT_FOV` = 45).
  - Los sliders cambian la `Camera` al momento, así que el efecto se ve.
  - Recuerda los valores que tenía la cámara al abrirse: `discard()` los repone, y hay cambios sin guardar si la cámara ya no coincide con ellos.
  - Tiene las claves y `applySettings`/`storeSettings`. `main` llama a `CameraMenu::applySettings` al arrancar.
- **`AudioMenu`** ("Audio") tiene, de momento, un único slider: el **volumen general** (0–100 %, de 5 en 5, `SoundEngine::setMasterVolume`), que escala todo lo que suena (la música y las voces).
  - Cambia el volumen al momento, así que se oye; recuerda el volumen que tenía el motor al abrirse: `discard()` lo repone, y hay cambios sin guardar si ya no coincide. Por defecto, 100 %.
  - Clave `audio.master_volume` (fracción de 0 a 1; un valor ausente, no numérico o fuera de rango se deja en el valor por defecto o se limita). `main` llama a `AudioMenu::applySettings` al arrancar, justo después de crear el `SoundEngine`.
- **`ControlsMenu`** ("Controles") muestra por grupos todas las acciones con su tecla, además de las entradas fijas (ratón, Esc, clic), y **permite cambiar las teclas** sobre una copia. Ver [Controles y teclas](#controles-y-teclas).
- Los menús oscurecen el juego y no tienen botón de cerrar.
- **El mundo no se detiene** con el menú abierto: `stage.update` sigue corriendo (el satélite termina de girar, por ejemplo). Solo se pausan los controles del jugador.
- **Los ajustes solo se guardan con "Guardar"** (o "Guardar y salir"), entre sesiones (ver [Configuración guardada](#configuración-guardada)).
- **Para añadir una opción a Cámara:**
  1. Si el valor no está en una clase, dale un getter y un setter que la apliquen al momento (como `Camera::setFov`).
  2. Añade un `UISlider` (o un `UIButton`) en el constructor de `CameraMenu`, con sus límites como constantes de la clase.
  3. Añádela al valor recordado al abrir, y a `hasUnsavedChanges`, `discard` y `resetToDefaults`.
  4. Dale una clave (`..._KEY`) y añádela a `applySettings` (con su valor por defecto y limitada a su rango) y a `storeSettings`.
- **Para una pantalla de ajustes nueva** (`AudioMenu` es la más corta, un buen modelo): hereda de `SettingsMenu`, implementa `hasUnsavedChanges`/`apply`/`discard`/`resetToDefaults`, termina el constructor con `addFooter()` y añade su botón en `OptionsMenu`.

## Configuración guardada

- **`Settings`** (`Settings.h`) es un almacén `clave = valor` en un fichero de texto (`#` inicia comentario), por defecto en **`$XDG_CONFIG_HOME/3dengine/settings.cfg`** (normalmente `~/.config/3dengine/settings.cfg`). Está fuera del repositorio porque son los ajustes de cada jugador.
- `load()`: si el fichero no existe, se usan los valores por defecto (primera vez); las líneas inválidas se saltan con un aviso. `save()` crea la carpeta, escribe un `.tmp` y lo renombra, así que un fallo nunca deja el fichero a medias.
- Las claves desconocidas se conservan al guardar: una versión antigua del juego no borra lo que guardó una más nueva.
- Valores: `getFloat`/`setFloat` y `getString`/`setString`. Un valor que no es un número devuelve el valor por defecto.
- **Claves actuales:** `camera.sensitivity` (multiplicador, 1 = `SENSIVILITY`) y `camera.fov` (grados), definidas por `CameraMenu`, que se guardan con su botón "Guardar". `audio.master_volume` (0 a 1), definida por `AudioMenu`. También `controls.<id>` (código de tecla GLFW), definidas por `Controls`, que se guardan con el botón "Guardar" de `ControlsMenu`.
- **Para guardar algo nuevo:** elige una clave con prefijo (`audio.master_volume`, por ejemplo), léela al arrancar y escríbela cuando cambie, seguido de `settings.save()`. El objeto `Settings` vive en `main` y llega a los menús por `MenuContext`.
- **En las pruebas**, lanza el juego con `XDG_CONFIG_HOME=<carpeta temporal>` para no sobrescribir la configuración real del usuario.

## Consola de comandos (tecla T)

- **T** (`Action::Console`, reasignable; sin paneles abiertos) o **escribir `/`** (`ui.bindChar('/')`: el carácter, esté en la tecla que esté según la distribución; en un teclado español es Mayús+7) abren; con `/` la caja empieza con la `/` escrita. Abren `CommandConsole : UIPanel` abajo, a lo ancho de la pantalla: las últimas 6 líneas que ha mostrado y una caja de texto (`UITextField`). Intro ejecuta lo escrito, Retroceso borra (se repite si se mantiene), ↑/↓ recorren los comandos ya escritos (el historial vive en `main` y dura toda la partida) y Esc la cierra. Como es un panel, los controles se pausan mientras está abierta (escribir "w" no anda).
- **Texto:** `UIManager` instala también el *char callback* de GLFW y pasa cada carácter al `onChar` del panel de arriba, en el mismo orden que las teclas (una sola cola, `InputEvent`). El carácter de una tecla que ha ejecutado un atajo se descarta (`dropChar`), aunque llegue en un frame posterior, como pasa con un método de entrada (IBus): hasta la siguiente pulsación de tecla. Si no, la T que abre la consola se escribía en ella. Sin paneles, un carácter puede tener su propio atajo (`bindChar`). `UITextField` solo guarda ASCII imprimible (lo que dibuja la fuente).
- **Comandos:** `Commands` (`core/`) es la lista de comandos. **Se escriben con `/` delante** (`/reset`); un texto sin `/` no se ejecuta ni responde nada (solo se repite la línea); uno desconocido responde "Comando desconocido: /x". Cada uno tiene nombre (se registra sin la `/`; no distingue mayúsculas), una línea de ayuda y una función que recibe las palabras siguientes y devuelve una línea para mostrar. `main` los registra.
  - **`/reset`**: vuelve a crear el mapa actual desde cero, como al arrancar el juego (objetos, jugador, RV, hora del día, música y vista), y apaga los modos de depuración. No se hace dentro del `update` de la UI: marca `resetRequested` y el bucle de `main` llama a `switchMap(currentMap)` al principio del frame siguiente, igual que un cambio de mapa (que cierra la consola).
- **Para añadir un comando:** `commands.add("nombre", "ayuda", [&](const std::vector<std::string> &args) { ...; return std::string("respuesta"); });` en `main`. Si cambia el mapa u otra cosa que no se pueda tocar desde la UI, márcalo y hazlo al principio del bucle.
- **Tiempo tras cargar un mapa:** al terminar `switchMap`, el bucle reinicia el reloj (`lastTime`, `dt = 0`). Antes, el tiempo de carga llegaba al frame siguiente como un único paso enorme de física, y tras un `reset` el jugador aparecía 3 m desplazado.

## Controles y teclas

**`Controls`** (`Controls.h`) es la única fuente de las teclas del juego. Cada acción (`enum class Action`) tiene una tecla (`key(action)`), una descripción (`describe`) y un grupo (`group`). `keyName` da el nombre legible, adaptado a la distribución del teclado. Hay una sola instancia, creada en `main`, que usan:
- `Controller`: las teclas de movimiento;
- `InteractionSystem`: Usar, y también el texto del aviso;
- `PauseMenu`: Salir;
- el atajo del selector de mapas (en `test.cpp`);
- `ControlsMenu`, que lo lista y lo cambia.

| Acción | Tecla por defecto | Dónde se lee |
|---|---|---|
| `MoveForward/Back/Left/Right` | W / S / A / D | `Controller::update` (cada frame) |
| `Use` | E | `InteractionSystem::update` |
| `Engine` | R | atajo `ui.bindKey` en `test.cpp`: llama a `GameStage::toggleEngine()` (`TestStage` solo actúa si el jugador va en el RV): enciende o apaga el motor |
| `VehicleCamera` | C | atajo `ui.bindKey` en `test.cpp`: llama a `GameStage::toggleVehicleCamera()` (`TestStage` solo actúa si el jugador va en el RV): cambia entre la cabina y la vista exterior |
| `Headlights` | F | atajo `ui.bindKey` en `test.cpp`: llama a `GameStage::toggleHeadlights()` (`TestStage` solo actúa si el jugador va en el RV) |
| `LeaveVehicle` | Mayús izquierda | atajo `ui.bindKey` en `test.cpp` (sin paneles abiertos): llama a `GameStage::leaveVehicle()` |
| `Quit` | X | `PauseMenu::onKey` y el texto de su botón |
| `Maps` | Z | atajo `ui.bindKey` en `test.cpp` (con la tecla leída en cada pulsación) y `MapSelector` (que se cierra con su misma tecla) |
| `DebugSelect` | 1 | atajo `ui.bindKey` en `test.cpp`: enciende y apaga el modo selección del `DebugSelector` (y el texto de su recuadro) |
| `DebugPlace` | 2 | atajo `ui.bindKey` en `test.cpp`: enciende y apaga el modo colocación del `DebugSelector` |

Ya no hay acciones para subir y bajar (eran "sin gravedad"): todo camina con gravedad. `PlayableCharacter::control` (de `main`) mantiene su parámetro `up`, y `Controller` le pasa 0.

**Fijas (no son `Action`):** Esc es la tecla genérica de "atrás" de la interfaz (`UIManager`): cierra el panel de arriba y abre la pausa. El ratón mira, y el clic izquierdo usa los paneles. Aparecen en `Controls::fixedControls()` para la pantalla de ayuda. (La fila de los clics de los modos de depuración se quitó para que cupiera la de la consola: la pantalla de Controles llena una ventana de 600 px de alto, y lo que hace el ratón en esos modos ya lo dice su recuadro.)

**Regla: no escribas `GLFW_KEY_...` para una función del juego.** Añade una `Action`, con su tecla en el constructor de `Controls`, su texto en `describe`, su grupo en `group` y su **`id`** (el nombre en el fichero, que no hay que cambiar después), y léela con `controls.key(Action::...)`. Así aparece en la pantalla de controles, se puede reasignar y se guarda.
- Para un atajo sin panel abierto, usa la versión de `ui.bindKey` que recibe una función: `ui.bindKey([&]{ return controls.key(Action::X); }, acción)`. La tecla se consulta en cada pulsación, así que sigue a los cambios.
- `bindKey(int)` es para teclas fijas, como Esc.

**Reasignar teclas (`ControlsMenu`):**
- Al hacer clic en una acción (una `UIInfoRow` clicable), la fila muestra "Pulsa una tecla..." y la siguiente tecla (`onKey`) se le asigna. Esc cancela la captura (`Controls::isBindable` excluye Esc).
- **Sin duplicados:** si la tecla ya era de otra acción, se intercambian (`assign`), y un mensaje lo indica.
- **Los cambios se hacen en una copia** (`edited`). **Guardar** la copia a `context.controls` (el juego la usa al momento, porque todo lee `Controls` cada vez), `writeTo(settings)` y `settings.save()`. **Por defecto** pone las teclas de fábrica en la copia; hay que guardar después.
- **Volver** (o Esc) vuelve a Opciones. Si `hasUnsavedChanges()` (la copia es distinta de los controles en uso), abre antes un `ConfirmDialog` con "Guardar y salir" y "Salir" (descarta los cambios). Esc sobre el aviso lo cierra y vuelve a los controles.

**Guardado:** en `Settings`, como `controls.<id> = <código GLFW>`; por ejemplo, `controls.move_forward = 87` es la W. `main` lo lee al arrancar con `controls.readFrom(settings)`, que valida el conjunto entero: admite teclas intercambiadas, ignora las inválidas, Esc y las repetidas (por un fichero editado a mano), y en esos casos esas acciones conservan su tecla por defecto.

## Mapas y selector de depuración

- Los mapas están en la lista `maps` de `main` (`test.cpp`). Cada uno tiene un **nombre** y una **función que crea su `GameStage`**, que devuelve `nullptr` si falla. Hoy son "Desierto de dia" (`TestStage`) y "Desierto de noche" (`SceneStage` con `desert.scene`).
- **Z** (`Action::Maps`, sin paneles abiertos) abre `MapSelector`, con un botón por mapa; el actual aparece marcado "(actual)". Z o Esc lo cierran.
- **Cambiar de mapa es diferido:** el botón solo apunta el índice (`requestedMap`). El bucle principal llama a `switchMap` al principio del frame siguiente, nunca dentro de un callback de la UI, porque el botón que se pulsó todavía se está ejecutando.
- **`switchMap(i)`** crea el mapa nuevo (si falla, avisa y se queda en el actual). Después:
  1. cierra todos los paneles (`ui.closeAll`), porque pueden apuntar a objetos del mapa viejo;
  2. vacía el `InteractionSystem`, olvida el objeto elegido del `DebugSelector` (`clear`) y destruye el mapa viejo;
  3. registra los interactuables nuevos y **arranca la música del mapa** (`MusicPlayer::play`; sin música, silencia la anterior);
  4. conecta el `Controller` al nuevo jugador, con la cámara que pide el mapa y mirando al frente;
  5. aplica su entorno (luz, `moonDir`, `fogColor`).
- **Regla:** nada fuera del mapa puede guardar punteros a sus objetos sin limpiarlos en `switchMap`.

**Añadir un mapa:**
- **En datos (lo más fácil):** crea `assets/scenes/mi_mapa.scene` (ver [Ficheros de escena](#ficheros-de-escena-assetsscenesscene)) y añade a `maps` la entrada `{"Mi mapa", [floorMode]() -> std::unique_ptr<GameStage> { return SceneStage::load("../assets/scenes/mi_mapa.scene", "../assets", floorMode); }}`. El visor web también lo mostrará con `--scene`.
- **En código** (si necesita lógica propia, como `TestStage`): hereda de `GameStage` y, en el constructor, rellena `environment`, `cameraDistance`/`cameraHeight` y `player`, llama a `setFloor` y a `add`/`addDynamic` para el contenido, y opcionalmente `setSky` e `interactables`. Si hace falta, sobrescribe `apply()`. Después añádelo a `maps`.

## Modos selección, colocación y propiedades (depuración)

`DebugSelector` (`src/debug/`) sirve para inspeccionar los objetos del mapa mientras se juega, moverlos y cambiar sus valores. Tiene tres modos (`Mode::Select`, `Mode::Place`, `Mode::Inspect`); cada tecla enciende el suyo, o lo apaga si ya estaba encendido. En todos se ve una cruz en el centro y un recuadro arriba a la izquierda.

### Selección (tecla 1)

- **1** (`Action::DebugSelect`, reasignable; sin paneles abiertos).
- **Clic izquierdo:** lanza un rayo desde la cámara hacia el centro de la vista y elige el objeto **visible y colisionable** más cercano que toca (`CollisionShape::raycast`: cápsula o caja orientada exactas). El suelo y la carretera no se pueden elegir (no son colisionables), ni el jugador en primera persona (está oculto). Si el rayo pasa bajo una duna antes de llegar al objeto (se comprueba con `floorAt` cada 0.25 m), no elige nada. Un clic en el vacío deja la selección vacía.
- **Clic derecho:** elige al jugador (el `Walker`, o el RV si se conduce).
- **Qué muestra** (se refresca cada frame): el nombre (clase, nombre de `Interactable` si lo tiene e índice en el stage: `#n` estáticos, `#dn` dinámicos) y lo que da **`GameObject::describe(lines)`**, un método virtual en el que cada clase añade sus líneas tras las de su padre:
  - `GameObject`: posición, rumbo e inclinación, escala, visible, colisionable, modelo, forma (radio y alto de la cápsula, o medidas y centro de la caja) y la AABB (mínimo, máximo y tamaño);
  - `DynamicGameObject`: velocidad, aceleración, masa, gravedad, velocidad máxima, rozamiento y si está en el suelo;
  - `RV`: ocupado, acelerador y volante, velocidad hacia delante, velocidad angular, centro de masas y la longitud de la suspensión de cada rueda (`*` = toca el suelo).
  - El selector añade la altura y el material del suelo bajo el objeto y su distancia a la cámara.
- **En el mundo** dibuja con un `LineRenderer` la forma de colisión (naranja), la AABB (azul) y la velocidad (verde, lo que recorre en 0.5 s). Las líneas se ven a través de todo.
- **No es un panel**, sino un `UIOverlay` (`UIManager::addOverlay`): no pausa el `Controller`, así que se puede andar o conducir con un objeto elegido y ver su física en directo. Los paneles se dibujan encima.
- Solo guarda un `weak_ptr` del objeto, y `switchMap` lo vacía (`clear`).
- **Para que una clase nueva muestre sus datos,** sobrescribe `describe` llamando primero a la de su padre. Usa `textFormat`/`textOf` (`core/TextFormat.h`).

### Colocación (tecla 2)

- **2** (`Action::DebugPlace`) pasa a colocar **el objeto elegido** con la 1 (si no hay ninguno, el recuadro lo dice).
- El destino es el **punto del suelo bajo la cruz**: `floorHit` avanza por el rayo de la cámara en pasos de 0.25 m hasta pasar bajo el suelo y afina por bisección. El objeto conserva **su altura sobre el suelo** (los adornos hundidos 5 cm siguen hundidos; la cabeza del satélite sigue sobre su poste).
- Se dibuja la silueta del objeto en el destino (blanco) y una línea desde donde está. El recuadro muestra el destino y la posición actual.
- **Clic izquierdo:** `Stage::relocate(objeto, destino)` (ver "La rejilla"). Se puede seguir haciendo clic para moverlo otra vez. Para elegir otro, **1** vuelve al modo selección.
- **Botón derecho pulsado + mover el ratón a los lados:** gira el objeto alrededor de la vertical (`Stage::turn`), con la sensibilidad de la cámara (a la derecha = sentido horario visto desde arriba, como gira la vista). Mientras está pulsado, la cámara no gira: `capturesMouse()` lo dice y el bucle de `main` llama a `controller.setLookEnabled(...)` (el `Controller` sigue leyendo el ratón, así que al soltar no salta; las teclas siguen moviendo al personaje). El recuadro muestra el rumbo ("girando" mientras tanto).
- **Con Mayús (cualquiera de las dos) además:** el rumbo se ajusta a múltiplos de 15° (`SNAP_STEP`): 0°, 15°, 30°… absolutos, no pasos desde donde estaba. El selector acumula el giro del ratón sin ajustar (`turnHeading`) y gira el objeto hasta el múltiplo más cercano por el camino más corto; al soltar Mayús, vuelve al ángulo libre. Mayús es un modificador fijo (como los botones del ratón, en `fixedControls`), no una `Action`. Como Mayús izquierda es también `LeaveVehicle`, ese atajo no hace nada mientras `capturesMouse()` (pulsa el botón derecho antes que Mayús si estás conduciendo).
- El rumbo sale de **`GameObject::getHeading()`** (virtual: por defecto, hacia dónde apunta su +z; `Satellite` lo calcula de su azimut, porque su rotación incluye la inclinación del cénit). `turn(r)` le suma `r`.
- No se comprueba si el destino está libre: si cae encima de otro objeto, las colisiones lo apartan en el frame siguiente como siempre (un dinámico se mueve; entre dos estáticos no se hace nada). El poste del satélite, que es un objeto aparte, se puede elegir y mover solo.

## Audio y voz (TTS)

```
EspeakSynthesizer : SpeechSynthesizer   texto UTF-8 → AudioClip (proceso espeak-ng, sin shell)
          │ (en otro hilo, std::async)
        Voice : LineNarrator               say(texto) → sintetiza → Sound espacial; progress() 0..1
          │                                 (sin audio: "lee" en silencio a CHARS_PER_SECOND)
     SoundEngine (miniaudio)               mezcla los Sound; oyente = cámara (setListener cada frame)
          ▲
         Npc : DynamicGameObject, Interactable
           Dialogue(frases, voice): UITextBlock(visible = voice.progress()) + Siguiente / Cerrar
```

- **`SoundEngine`:** hay uno por juego, creado en `main` después de la ventana. miniaudio elige el backend (PulseAudio/PipeWire, ALSA) al arrancar y no añade nada al enlazado salvo `-ldl -lpthread -lm`. Si no hay dispositivo de audio, el juego funciona en silencio: `isAvailable()` es `false` y `play()` devuelve `nullptr`.
  - `play(clip, spatial, posición)` devuelve un `Sound`, que suena mientras exista: destruirlo lo para. Los sonidos espaciales se atenúan entre 1.5 y 40 unidades y se oyen a izquierda o derecha según dónde estén.
  - **El motor tiene que vivir más que cualquier `Sound`:** en `main` se declara antes que el stage.
- **`AudioClip`:** muestras `float` en memoria. `loadWav` lee WAV PCM de 8/16/32 bits o float, incluidos los que se generan en streaming sin tamaño, como el de espeak-ng.
- **`SpeechSynthesizer`** es abstracta. `synthesize(texto, VoiceSettings)` es bloqueante y debe poder llamarse desde varios hilos. `VoiceSettings` incluye el idioma (`"es"`), las palabras por minuto y el tono.
  - **`EspeakSynthesizer`** ejecuta `espeak-ng -v <idioma> -s <ppm> -p <tono> --stdin --stdout` con `fork`/`exec` y le pasa el texto por la entrada estándar: sin shell, así que cualquier texto es seguro. Tarda unos 15 ms por frase.
  - **Es una dependencia en tiempo de ejecución:** si `espeak-ng` no está instalado, la síntesis falla y las voces "leen" en silencio. Además, ignora `SIGPIPE` en todo el proceso.
  - La voz `es` de espeak-ng suena robótica. Las voces MBROLA (`mb-es1`…) necesitan el paquete `mbrola-es*`.
- **`Voice`:** `say()` vuelve al momento, porque sintetiza con `std::async` y reproduce cuando el resultado está listo (en `update`). `progress()` es la posición de reproducción dividida por la duración, y sirve para revelar los subtítulos. `stop()` la calla. Al destruirse espera a la síntesis pendiente (no se puede cancelar).
- **`Npc`:**
  - Al abrirse su panel (`onInterfaceOpened`, con la posición del jugador) se gira hacia él y dice la primera frase: cada conversación empieza desde el principio.
  - El botón dice "Siguiente" y, en la última frase, "Cerrar", que cierra el diálogo (`panel.requestClose()`). Su texto se lee en cada frame (`UIButton` con texto dinámico).
  - Esc cierra el diálogo en cualquier momento. Al cerrarse, de cualquier forma (`onInterfaceClosed`), la voz se corta.
  - La voz sale de `MOUTH_HEIGHT` sobre su posición.
  - Su `AnimatedModel` debe crearse con `feetAtOrigin = true`, para que esté de pie sobre su posición: es un `DynamicGameObject` con gravedad y el stage lo apoya en el suelo.
  - Hoy hay uno, "Pingu", en `TestStage`, delante del inicio.
- **Subtítulos:** `UITextBlock` ajusta primero el texto entero (así las palabras no saltan de línea mientras aparecen) y muestra solo la fracción `voice.progress()` de sus caracteres.

## Música de fondo

Cada `Stage` tiene una música de fondo (`Stage::setMusic(clip, loop = true, volume = 1)`; `loadMusic(ruta)` la carga de un WAV). **`nullptr` significa sin música**, que es lo que tiene por defecto. El stage solo la guarda: quien la reproduce es el `MusicPlayer`, que crea `main` después del `SoundEngine`.

- **Cuándo suena:** `switchMap` llama a `music.play(stage->getMusic(), stage->isMusicLooping(), stage->getMusicVolume())` con cada mapa nuevo. La pista anterior se para siempre (si el mapa nuevo no tiene música, queda silencio) y la nueva empieza desde el principio. La música sigue sonando con los menús abiertos.
- **En bucle por defecto:** `Sound::setLooping` (miniaudio) la repite hasta que se para. `desert.wav` está hecha para que el bucle no tenga salto (ver el pipeline de assets). Cada `Stage` tiene además un **sonido ambiente** (`setAmbience`, otro `MusicPlayer` en `main`) que suena en bucle junto a la música y no baja con ella: `TestStage` pone `wind.wav`.
- **No es espacial:** se oye igual en los dos oídos, sin atenuarse con la distancia (`play(clip, spatial = false)`).
- **Mapas:** "Desierto de dia" (`TestStage`) carga `assets/music/desert.wav` en su constructor; "Desierto de noche" (`SceneStage`) no tiene música, de momento (los `.scene` no tienen un comando para ello).
- **Memoria:** el clip son muestras `float` (unos 20 MB para 76.8 s a 32 kHz estéreo) y se vuelve a cargar cada vez que se entra en el mapa. Si falla la carga, el mapa se queda sin música y avisa por la salida de error.

## Diálogos: hablar y leer

Un NPC y un cartel muestran la misma caja de diálogo. Lo único que cambia es cómo se entrega cada línea:

```
Dialogue (frases, página actual, panel)  ──usa──▶  LineNarrator (abstracta)
   ▲                ▲                                 ├── Voice       TTS con sonido (Npc)
  Npc            Readable                             └── Typewriter  letra a letra, sin sonido (Readable)
```

- **`LineNarrator`:** `say(texto)`, `stop()`, `update(dt, posición)`, `progress()` (0..1 de la línea entregada), `isSpeaking()` e `isPreparing()` (por ejemplo, mientras se sintetiza la voz).
- **`Typewriter`:** revela `DEFAULT_SPEED` = 40 caracteres por segundo (configurable), sin sonido. `stop()` deja la línea completa.
- **`Dialogue(frases, narrador, textoOcupado)`** construye el panel con `buildPanel(panel)`:
  - el `UITextBlock` con la línea, revelada según `narrator.progress()`;
  - "n/N", con "..." mientras se prepara y `textoOcupado` mientras habla ("hablando" en el NPC; vacío en el cartel);
  - el botón "Siguiente", que en la última línea pasa a "Cerrar" y cierra el panel (Esc también lo cierra).

  `start()` empieza desde la primera línea y `end()` calla al narrador. Su dueño los llama desde `onInterfaceOpened`/`onInterfaceClosed`.
- **`Npc`** = `Voice` + `Dialogue(frases, voice, "hablando")`. **`Readable`** = `Typewriter` + `Dialogue(páginas, typewriter)`.
  - En los dos, **el narrador se declara antes que el `Dialogue`**, que guarda una referencia a él (orden de construcción).
  - Los dos llaman a `narrator.update` en su `update(dt)`.
- **Verbo del aviso:** `Interactable::getInteractionVerb()` (por defecto "usar"). El NPC usa "hablar con" y el cartel "leer", así que el aviso dice "E: leer Cartel".
- **El cartel:** `assets/sign/` (`generate_sign.py`). Es un poste con un tablón de madera y "AVISO" pintado. El origen está al pie del poste y el tablón mira hacia +z, a unos 1.3 de altura. En `TestStage` está en (7.5, suelo, −1), lejos del satélite, para que el más cercano no sea siempre el satélite.

**Añadir algo que se lee** (en el constructor del mapa; no necesita audio):
```cpp
auto nota = make_shared<Readable>(loadModel("../assets/sign/sign.obj"), "Cartel",
                                  std::vector<std::string>{"Página 1", "Página 2"},
                                  1.3f);          // altura del texto sobre su posición
nota->setPosition(x, groundAt(x, z), z);  nota->setYaw(...);
add(nota);  interactables.push_back(nota.get());
```

**Otra forma de entregar el texto** (por ejemplo, voz pregrabada en ficheros WAV): crea una subclase de `LineNarrator` y pásasela a un `Dialogue`.

**Añadir un NPC:** en el constructor del mapa (necesita el `SoundEngine` y el `SpeechSynthesizer`; mira cómo se los pasa `main` a `TestStage`):
```cpp
VoiceSettings voz; voz.pitch = 40;                       // cada NPC puede sonar distinto
auto npc = make_shared<Npc>(make_shared<AnimatedModel>("../assets/...fbx", true),
                            "Nombre", std::vector<std::string>{"Frase 1", "Frase 2"},
                            sound, speech, voz);
npc->setPosition(x, groundAt(x, z), z);  npc->setGravity(25.0f);
addDynamic(npc);  interactables.push_back(npc.get());
```

**Cambiar de motor de voz:** crea otra subclase de `SpeechSynthesizer` (por ejemplo, una voz neuronal que devuelva un WAV) y usa esa en `main` en lugar de `EspeakSynthesizer`. No hay que tocar nada más.

**Probar el audio sin molestar al usuario** (y sin que se mezcle con otras aplicaciones):
1. Crea una salida virtual: `pactl load-module module-null-sink sink_name=engine_test`.
2. Lanza el juego con `PULSE_SINK=engine_test`.
3. Graba solo esa salida: `parec --device=engine_test.monitor --format=s16le --rate=8000 --channels=1 > x.raw`, y mide la energía (RMS) por tramos con numpy.
4. Al terminar, `pactl unload-module <id>`.

Grabar `@DEFAULT_MONITOR@` recoge también lo que el usuario esté escuchando.

## Hora del día y ciclo de luz

Todo `Stage` lleva un reloj: `getTimeOfDay()` (horas, 0 = medianoche, 12 = mediodía, da la vuelta a las 24) y `getDayDuration()` (segundos reales que dura un día; **0 = el tiempo está parado**, el valor por defecto). `Stage::update` lo avanza y llama a `setTimeOfDay`, que a su vez llama al gancho virtual `onTimeChanged()`. **Qué significa la hora lo decide cada mapa** sobrescribiendo ese gancho (luz y cielo, quién está despierto, una puerta que se abre...); el `Stage` solo cuenta el tiempo. En un `.scene` se fija con `time_of_day` y `day_duration`; en la línea de órdenes, `--time H` y `--day-duration S` pisan los del mapa (útil para depurar: `--time 22 --day-duration 0`).

`TestStage` (día) lo usa para un ciclo sencillo (`DAY_DURATION` = 360 s, empieza a las 12:00 —mediodía—, y anochece a los ~90 s (la puesta de sol es a las 18:00): el sol sale por +x a las 6:00, está más alto a las 12:00 y se pone por −x a las 18:00 (con una pequeña inclinación hacia −z). No hay luna. `onTimeChanged` rellena el `Environment` (`horizon`, `skyZenith`, `sunDir`, `starAlpha`, `lightDir`, `lightColor`) según la altura del sol: azul de día, naranja en el amanecer y el atardecer, y de noche oscuro con estrellas. La luz es la del sol mientras está alto y, siempre, un **mínimo** (`NIGHT_LIGHT` = 0.022, 0.025, 0.04) que de noche cae desde arriba: tan bajo que sin los faros del RV apenas se ve nada. El cielo es una cúpula sin textura (`assets/sky/skydome_plain.obj`, `unlit` = 3) que pinta el shader; el `main` envía el `Environment` al shader **cada frame** (`applyEnvironment`). **La música sigue al sol:** `onTimeChanged` también llama a `setMusicVolume` (de 0 con el sol 6° bajo el horizonte a 1 con él a 30° de altura: empieza a bajar por la tarde y casi no se oye al ponerse) y `main` pasa ese volumen al `MusicPlayer` cada frame. El viento (`assets/music/wind.wav`, `Stage::setAmbience`/`loadAmbience`, otro `MusicPlayer` en `main`) no se toca, así que de noche solo queda él. Para otro ciclo, otro mapa sobrescribe `onTimeChanged` y rellena el `Environment` como quiera.

## Suelo y colisiones

Todo esto vive en `Stage` (y en `physics/`). Un objeto nunca sabe nada del suelo ni de los demás: el stage lo mueve y lo empuja.

### El suelo

`Stage::setFloor(malla, posición)` (una sola vez) guarda la malla del suelo y construye su estructura de consulta. El modo se elige **al crear el stage** (`FloorMode`) y no cambia después; `--ray` en la línea de órdenes elige el segundo.

| Modo | Cómo busca la altura | Pros y contras |
|---|---|---|
| `HeightField` (por defecto) | La malla debe ser una rejilla regular en x/z. Se interpola la altura del triángulo de la celda (la misma diagonal que el generador de dunas). | O(1). No admite voladizos ni túneles. |
| `DownwardRay` | Un rayo vertical contra los triángulos, indexados en una rejilla 2D (unos 2 triángulos por celda). Con `maxY` se ignora lo que queda por encima del objeto (un techo, un puente). | Cualquier malla. Algo más lento. |

`Stage::floorAt(x, z, altura, normal, maxY)` da la altura y la normal. Los dos modos dan la misma altura (diferencia < 1e-5 en las pruebas).

### Materiales del suelo

Además de la altura y la normal, un `Stage` dice **de qué material es el suelo** en cada punto: `Stage::materialAt(x, z)`, que devuelve un `FloorMaterial` (hoy `Sand` y `Asphalt`; para otro material, añádelo al `enum` de `FloorMaterial.h` y a `floorMaterialName`). Quien conduce o anda sobre el suelo se comporta según el material: el RV va más lento sobre arena. Fuera del suelo, o sin información, la respuesta es `Sand`.

- **`MaterialMap`** es una rejilla de materiales extendida sobre todo el suelo. En una imagen PNG de 8 bits en gris cada píxel es un número de material (0 = arena, 1 = asfalto, los números de `FloorMaterial`): las columnas van a lo largo de +x y las filas, de +z (la primera fila es el borde de z mínima), y la imagen cubre exactamente los límites x/z del suelo. `MaterialMap::loadImage(ruta)` la lee y `MaterialMap::uniform(material)` hace una de un solo material.
- **`setFloor(malla, posición, materiales)`:**
  - Con `FloorMode::HeightField` **el mapa es obligatorio**: un campo de alturas solo sabe alturas, así que sin mapa `setFloor` falla y el stage se queda sin suelo. Un suelo de un solo material pasa `MaterialMap::uniform(...)` (así hace `SceneStage`, con arena).
  - Con `FloorMode::DownwardRay` es opcional: sin mapa, el material de cada triángulo sale del nombre de su material en el fichero de la malla (`floorMaterialFromName`: `road` y `asphalt` son asfalto, cualquier otro nombre es arena). Si se da un mapa, manda el mapa.
- **El mapa del desierto de día** (`assets/desert/dunes_loop_materials.png`, 360 × 360 píxeles de 0.5 m) lo genera `generate_assets.py` junto al terreno y la carretera: asfalto donde está la cinta de la carretera (8 m) y arena en el resto. Comprobado contra la geometría de la carretera en 20 000 puntos sin ningún fallo, en los dos modos de suelo.

### Formas de colisión

Cada `GameObject` tiene una `CollisionShape`:

- **`Capsule`** (por defecto): una pastilla vertical. Se ajusta a la caja del modelo (`Capsule::fit`); para un modelo animado es una de 0.4 de radio y 1.8 de alto, con la base en la posición del objeto (los pies).
- **`Box`**: una caja orientada. Se pasa **al construir** el objeto, como último argumento opcional del constructor. El `RV` lo hace así (2.5 × 7.4, desde 0.5 de altura hasta el techo).
- Las dimensiones se dan en el sistema del objeto y se escalan, giran y trasladan con su `Pose`.
- `CollisionShape::collide` prueba cualquier par (cápsula-cápsula, cápsula-caja, caja-caja con el teorema del eje separador) y devuelve la **normal de salida y la profundidad**, que es el empujón mínimo que los separa.
- `setCollidable(false)` hace que el stage ignore la forma. **El suelo y la carretera deben ir así**, o serían un obstáculo gigante.

### La rejilla

El `Stage` divide el plano x/z en celdas fijas cuadradas (8 unidades por defecto, segundo argumento del constructor). Cada objeto se registra en las celdas que cubre su caja envolvente. **Solo se comparan los objetos que comparten celda**, y nunca dos estáticos entre sí.

- Los estáticos (`add`) se colocan en la rejilla **al añadirlos**. Para moverlos después, usa **`Stage::relocate(objeto, posición)`**, nunca `setPosition`: llama a `GameObject::teleport` y vuelve a colocar los estáticos en la rejilla (son pocos: se rehace entera).
- `teleport` es virtual: `DynamicGameObject` además se para; el `RV` coloca su `VehicleBody` allí (derecho, con su rumbo y en reposo); `Satellite` se lleva el poste.
- Igual para girar: **`Stage::turn(objeto, radianes)`** llama a `GameObject::turn` (virtual: gira la rotación alrededor de la vertical del mundo, conservando inclinación y escala; el `RV` recoloca su cuerpo derecho con el rumbo nuevo; `Satellite` cambia su azimut, y el del objetivo, y el poste no gira) y rehace la rejilla si es estático. Un estático que cubre más de 1024 celdas se ignora con un aviso (casi seguro es el suelo, que debería ser no colisionable).
- Los dinámicos (`addDynamic`) se recolocan en la rejilla cada frame.
- `getShapeTests()` dice cuántos pares se probaron en el último frame, para ver cuánto ahorra.

### Un frame de física (`Stage::update(dt)`)

1. Cada estático: `update(dt)`.
2. Cada dinámico: `update(dt)`, después `apply(objeto, dt)` (la regla del escenario, que llama a `collideWithFloor`) y después la comprobación **forma contra suelo**.
   - `collideWithFloor` mantiene el objeto dentro de los límites del suelo, lo sube si se hundió y lo pega al suelo en las bajadas. Si el objeto lleva su propia lógica (`contactFloor` devuelve `true`), como el RV, el stage se la deja.
3. **Forma contra suelo:** los puntos más bajos de la forma (`floorSamples`) no pueden quedar bajo el suelo. Una caja da sus ocho esquinas y una rejilla de puntos en su cara más baja *en el mundo*, así que vale **de cualquier lado que esté boca arriba**. Si algo se hunde, se empuja el objeto hacia arriba con `applyCollision`. El rayo solo cuenta suelo dentro de la altura del propio objeto.
4. `resolveCollisions()`: coloca los dinámicos en la rejilla y prueba los pares de cada celda (dos pasadas, porque un empujón puede meter un objeto en otro).
   - Contra un estático se mueve solo el dinámico. Entre dos dinámicos se reparte el empujón según la masa (`getMass`): el más ligero se mueve más.
   - Se quita la velocidad con la que se acercaban (sin rebote).
   - `applyCollision(empuje, cambioDeVelocidad)` es virtual: el RV lo sobrescribe para mover también su cuerpo de física.
5. Otra vez forma contra suelo, por si un empujón metió algo en el terreno.

### Propiedades (tecla 0)

- **0** (`Action::DebugInspect`, reasignable).
- **Sin hacer clic:** el objeto bajo la cruz (el mismo rayo que en selección, `pick`) se resalta con su forma y el recuadro lista sus **propiedades** con su valor actual, cada frame. Lo que se apunta no cambia la selección de la tecla 1 (`hovered` es aparte de `selected`).
- **Clic izquierdo:** abre una ventana (`PropertyPanel : UIPanel`, a la derecha) para cambiarlas; **clic derecho:** la del jugador (el RV si se conduce). Con la ventana abierta los controles están en pausa (regla general de los paneles) y el recuadro de datos se oculta para no pisarla. Esc o "Cerrar" la cierran.
- **Las propiedades** son `Property` (`world/Property.h`): `Number` (slider; solo texto si no tiene `set`), `Toggle` (botón que lo cambia), `Action` (botón que hace algo) e `Info` (texto). No guardan el valor: `get`/`set` son funciones atadas al objeto, así que siempre muestran el estado real. Las da **`GameObject::getProperties(props)`**, virtual, como `describe`: cada clase añade las suyas tras las de su padre.
  - `GameObject`: posición (texto) y visible.
  - `DynamicGameObject`: velocidad horizontal (al cambiarla conserva la dirección del movimiento, o la del objeto si está parado), velocidad máxima, rozamiento y gravedad.
  - `RV` (no usa las de `DynamicGameObject`: su movimiento es el del `VehicleBody`): velocidad hacia delante (cambia la del cuerpo), velocidad máxima (`VehicleBody::setMaxSpeed`), combustible, faros encendidos, **probabilidad de avería de los faros** (%/min), **probabilidad de que se apaguen** (%), su estado, "provocar una avería" y parabrisas roto.
  - `Satellite`: hacia dónde apunta, azimut y cénit objetivo y velocidad de giro.
- `PropertyPanel` guarda un `shared_ptr` del objeto mientras está abierta (sus controles están atados a él). **Para que una clase nueva tenga propiedades,** sobrescribe `getProperties` llamando primero a la de su padre.

## El RV: vehículo con suspensión

`RV` es el vehículo del mapa de día (el jugador lo conduce tras subir por su puerta, ver [Subir y bajar del RV](#subir-y-bajar-del-rv)). Su física está en `VehicleBody`, que no depende de OpenGL (se puede probar sola) y se mueve en pasos fijos de 1/240 s.

- **Chasis:** cuerpo rígido con masa (3000 kg), inercia y orientación (cuaternión). El **centro de masas está bajo los ejes de las ruedas** (peso en la parte baja) y la inercia es la de una caja baja, así que se resiste a volcar: en curvas rápidas se inclina unos 2°.
- **Ruedas:** cuatro muelles amortiguados (rigidez y amortiguación se calculan para que lleven el peso con un recorrido de reposo de 0.35 m, entre 0.15 y 0.55). Cada rueda lanza un rayo de suelo desde su anclaje (`floorAt`) y empuja el chasis con la fuerza del muelle. Las ruedas se dibujan como piezas aparte (`wheel_negx.obj`, `wheel_posx.obj`), así que **suben y bajan con la suspensión** y las delanteras giran al dirigir.
- **Neumáticos:** dan tracción, freno y agarre lateral, limitados por la carga de cada rueda (círculo de fricción). Las delanteras dirigen (menos ángulo a más velocidad), así que el giro sale de las fuerzas de los neumáticos. - **Motor y velocidad terminal:** el motor empuja con la misma fuerza a cualquier velocidad (`acceleration`, 14 m/s², con W/S) y la velocidad máxima (`setMaxSpeed`) es una **velocidad terminal**: lo que se opone al movimiento crece con la velocidad, de forma lineal (la rodadura de los neumáticos, por rueda) y **con el cuadrado de la velocidad** (la resistencia del aire, una fuerza `masa · k · |v| · v` sobre la velocidad horizontal, también en el aire). El coeficiente `k` se calcula en cada subpaso para que, en llano, el motor y la resistencia se igualen justo en `maxSpeed` de la superficie (`k = (acceleration − rolling · rodadura · maxSpeed) / maxSpeed²`): se llega a ella de forma asintótica (en asfalto, 20 m/s: 17 a los 2 s, 19.6 a los 3.5 s), sin pasarse salvo cuesta abajo o empujado. Marcha atrás usa solo la fuerza justa para su velocidad terminal (`reverseFactor` × `maxSpeed`, −8 m/s), así que acelera poco (~3 m/s²). Soltar el acelerador a toda velocidad frena fuerte al principio (~11 m/s² a 20 m/s) y cada vez menos.
- **Esquinas del chasis:** ocho puntos de contacto elásticos, con rozamiento, algo por fuera de la caja de colisión. Entran en juego antes que la corrección dura del stage, así que un RV volcado se desliza, rueda y se frena con fricción en vez de "flotar" sin rozamiento. La fuerza de cada uno está limitada para que un choque fuerte no lo dispare.
- **Autoenderezado (tentetieso):** una aceleración angular lo devuelve siempre a apoyarse en las ruedas: suave si está algo inclinado y fuerte pasado ~26°, amortiguada para que se asiente. En el aire solo funciona al 15% (no hay nada contra lo que empujar). Desde cualquier postura, incluso boca abajo, vuelve a las ruedas en 1 a 2 s.
- **Arena y asfalto:** cada rueda pregunta de qué es el suelo bajo ella (`Stage::materialAt`, a través de `VehicleBody::setSurfaceQuery`) y recibe una `VehicleBody::Surface` con tres multiplicadores de los parámetros del vehículo: el agarre de los neumáticos, la resistencia a rodar y la velocidad máxima del motor. El asfalto es la referencia (todo 1). **En arena** (`SAND_*` en `RV.cpp`): agarre 0.75, resistencia a rodar ×2.5 y velocidad máxima ×0.5. La velocidad máxima es la media de las cuatro ruedas, así que con medio coche en la arena va a medias y se desvía hacia el lado lento. Medido en llano (con el motor de velocidad terminal): 20 m/s en asfalto y 10 en arena (antes 8.6: el motor no compensaba la mayor rodadura de la arena); una vuelta al circuito a todo gas, 17.6 s por la carretera frente a 29.3 s si todo fuera arena.
- **Averías de los faros:** con los faros encendidos, en cada frame puede empezar una avería con probabilidad `lightFaultChance` (en % por minuto, convertida a la del frame con `1 − (1 − p)^(dt/60)`, así no depende de los FPS; 10 %/min por defecto). El jugador no ve ese número: solo la tecla 0 lo muestra y lo cambia. Durante la avería (0.6–2.2 s) las lámparas saltan al azar entre apagadas, tenues y fuertes cada 0.03–0.16 s (`lampLevel` escala el color de los focos y de las luces del salpicadero; las lentes y el salpicadero se ven si pasa de 0.5). Al acabar, vuelven o, el `lightOutChance` % de las veces (35 % por defecto), **se apagan**: el interruptor queda en apagado y hay que encenderlas otra vez (F). Encender o apagar a mano corta la avería. `startLightFault()` la provoca al momento. Medido sin ventana (2000 min simulados): 0.10–0.11 averías/min con 10 %/min (esperado 0.105) y se apagan el 0 / 30 / 100 % con 0 / 35 / 100 %.
- **Control:** `control()` solo guarda el acelerador (W/S) y la dirección (A/D); la dirección de la cámara se ignora a propósito. El `Stage` llama a `RV::contactFloor`, que avanza el `VehicleBody` y copia su posición y su orientación al objeto.
- **Constantes:** la masa, el centro de masas, las esquinas y la forma del chasis están en `RV.cpp`; los parámetros de suspensión, neumáticos y autoenderezado, en `VehicleBody::Params` (`VehicleBody.h`).

## Partículas

Un sistema de partículas muy básico (`effects/`): cada partícula es un disco, y su movimiento lo calcula `ParticleEmitter` (sin OpenGL) y lo dibuja `ParticleRenderer`.

- **`ParticleEmitter`:** nace a un ritmo (`setRate`, partículas por segundo; 0 = apagado) desde una posición (`setPosition`), dentro de un cono alrededor de una dirección (`setDirection`, con `spread` de semiángulo) y con una velocidad aleatoria entre `speedMin` y `speedMax` más una velocidad base (`setBaseVelocity`, por ejemplo la del propio vehículo). Cada partícula:
  - **cae por la gravedad** (`gravity`, m/s², hacia abajo) y la frena el aire (`drag`, 1/s, exponencial);
  - crece de `sizeStart` a `sizeEnd` (el radio del disco) y se desvanece (su opacidad baja al cuadrado con la vida);
  - desaparece al acabar su vida (entre `lifeMin` y `lifeMax`) o al llegar al suelo (`setGround`, que da la altura del suelo en x, z).
  - El ritmo no depende de los FPS, hay un máximo de partículas vivas (`maxParticles`) y todo es reproducible con la misma semilla. Los ajustes son un `ParticleSettings`.
- **`Stage::addEmitter`** registra un emisor: el stage lo actualiza cada frame (después de mover los objetos, así que su dueño ya lo ha colocado) y le da la función del suelo para que las partículas que lo alcanzan se vayan.
- **`ParticleRenderer`** (creado en `main` tras la ventana) dibuja todas las partículas de todos los emisores del mapa de una vez, **después del mundo y antes de la interfaz**:
  - cada partícula es un cuadrado que mira a la cámara (con `Camera::getRight/getUp/getViewProjection`) y su propio shader (`particle.vert`/`particle.frag`), que recorta un disco con el borde suave;
  - **mezcla con transparencia**, de la más lejana a la más cercana para que los discos solapados se mezclen bien, con **prueba de profundidad** (el terreno y los objetos los tapan) pero **sin escribir profundidad** (no tapan nada);
  - **iluminadas** con la luz del mapa (`setLighting(lightColor, lightDir, focos)`, que `main` llama cada frame con el `Environment` y los focos encendidos): el ambiente del shader del mundo (0.3) más el sol con un «half Lambert» contra la normal del suelo (arriba), y los focos (faros) sin normal; el resultado se limita a 1, así que nunca es más claro que el color de la partícula. De día se ve como antes, al atardecer se tiñe de naranja y de noche queda oscura como la arena (antes brillaba en blanco);
  - deja el programa y el estado de GL como los encontró; hasta 4096 partículas a la vez (si hay más, se omiten las más lejanas).
- **El polvo del RV:** cada rueda tiene su emisor (`RV::getDust()`, que `TestStage` da al stage). Cada frame, `RV::updateDust` mira si la rueda está en el suelo, de qué es el suelo bajo ella (`Stage::materialAt`) y a qué velocidad va: **solo en arena** y por encima de 1.5 m/s la rueda lanza polvo desde donde el neumático toca el suelo, **hacia atrás (contra el sentido de la marcha) y hacia arriba**, con un cuarto de la velocidad del RV; cuanto más rápido va, más polvo (45/s por rueda a 10 m/s, hasta 15 m/s). En asfalto, en el aire o parado, el ritmo es 0 y el polvo que ya salió termina de caer. Las constantes `DUST_*` y los ajustes del polvo (color pálido, discos que crecen hasta 1.3 m de radio, gravedad 7) están en `RV.cpp`. Medido, relativo al RV: unos 7.8 m/s hacia atrás y 3.1 m/s hacia arriba al nacer, hasta 1.7 m de altura, y sin partículas bajo el suelo.
- **Granos de arena del RV:** junto al polvo, cada rueda tiene un segundo emisor (`RV::getGrains()`, mismas condiciones y mismo `updateDust`) que lanza granos sueltos: discos de 3.5 cm, oscuros (más que la arena), opacos hasta el 85 % de su vida (`ParticleSettings::fadeStart`), con gravedad 16 y poco rozamiento, a 3–8 m/s en un cono más abierto (0.8 rad), 70/s por rueda a 10 m/s. Las constantes `GRAIN_*` y los ajustes están en `RV.cpp`.

## Subir y bajar del RV

En el mapa de día el jugador empieza siendo el pingüino a pie (`Walker`, primera persona). El RV es un `Interactable` que se usa directamente:

- **Subir:** junto a su puerta (el lado +x del modelo, un poco por detrás del centro; con el RV orientado a +z, la puerta queda hacia el punto de inicio) aparece "E: conducir la autocaravana". Con E, `RV::onUse` ejecuta la acción de entrada que le pone el mapa (`setEnterAction`) y `TestStage::enterRV()` hace esto:
  1. el pingüino deja de andar, se oculta, deja de ser colisionable (estaría dentro de la caja del RV) y no tiene gravedad;
  2. cada frame `apply()` lo coloca en el asiento del RV (`seatPosition()`, dentro de la carrocería), así que va donde vaya el RV;
  3. `setPlayer(rv, 12, 3.5, yaw)` entrega los controles y la cámara al RV, mirando por detrás (`headingYaw()`);
  4. el RV pasa a "ocupado" (no se puede volver a usar y no frena) y las interacciones se desactivan (`interactionsEnabled() == false`).
- **Cámara:** dos vistas (`RV::CameraView`), con la tecla `VehicleCamera` (C) para cambiar (se recuerda al bajar y volver a subir). **`Cockpit` (por defecto)**: la cámara está en los ojos del conductor (`eyePosition()`: x = 0.45 (conducción por la izquierda, del lado de la puerta), y = 2.4, z = 1.7 en el marco del RV) y **sigue al vehículo**: el rumbo y el **cabeceo** (si el RV sube el morro, la vista sube con él) por completo y solo la mitad del **alabeo** (`COCKPIT_ROLL` = 0.5 en `RV.cpp`: en las dunas el RV se inclina mucho y con todo el alabeo el horizonte bailaba). `RV::followCamera` descompone la rotación del RV en rumbo, cabeceo y alabeo y se la da a la cámara con `Camera::setCarrier(rotación)`: el `yaw`/`pitch` de la cámara pasan a ser **relativos al vehículo** (0, 0 = mirar hacia delante) y la matriz de la vista es `Rx(pitch) · Ry(yaw) · portadorᵀ`, así que sigue bien al RV aunque mires de lado. `Camera::attachTo` quita el portador. El ratón mira alrededor desde ahí; por eso `Controller::update` toma `yaw`/`pitch` de la cámara en cada frame (antes eran suyos). Al cambiar de vista con C la mirada se recentra. **`Chase`**: la de siempre, `Camera::attachTo(rv, 12, 3.5)` orbitando por detrás. Dentro de la cabina solo se ve el exterior por el **parabrisas**, que es translúcido (opacidad 0.15): el cuerpo del RV tiene dos huecos en su cara inclinada (`body_with_openings` en `generate_rv.py`), con el **salpicadero** detrás (ver «La cabina» más abajo; borde inferior del hueco a y = 1.8 y salpicadero a 1.65, para ver el suelo desde ~7 m delante); además tiene una **ventana a cada lado de la cabina**, a la altura del conductor (`SIDE_WINDOW` en el script: un trapecio de z = 1.3 (justo detrás del conductor) hasta casi el parabrisas, y de 1.85 a 2.5, con el borde delantero **paralelo a la inclinación del parabrisas**, con un montante estrecho en medio), con hueco real en las paredes y cristal translúcido (`sideglass`, opacidad 0.15); los espejos retrovisores van en el montante delantero, delante del cristal; las demás ventanas (las del salón, la puerta, la trasera) siguen opacas. Lo pegado a las paredes empieza `GAP` = 4 mm hacia fuera, para que desde dentro no parpadee.
- **Objetos del borde sin dibujar:** `Stage::setEdgeCulling(margen)` hace que `Stage::Draw` no dibuje los objetos (estáticos y dinámicos) cuya posición esté a menos de `margen` del borde del suelo (para no enseñar el final del mundo); `edgeCullExempt` los exime (`GameStage`: el jugador; `TestStage`: también el RV y el pingüino a pie). `TestStage` lo usa con `EDGE_CULL_MARGIN` = 12 m (8 de los 49 objetos estáticos); solo afecta al dibujo, no a la física ni a las colisiones. La criatura desaparece al acercarse al borde.
- **Parabrisas roto:** `ImpactDetector` (`physics/`, sin OpenGL) decide si un choque es violento y de frente. El `RV` le cuenta cada colisión (`applyCollision`: la velocidad hacia delante antes y después, su rumbo y hacia dónde lo empujó el choque) y su velocidad cada frame (`contactFloor`). Un choque es **frontal** si el empuje apunta hacia atrás, a menos de ~53° (`frontalCos` = 0.6), y el RV iba a más de 5 m/s (`minSpeed`): entonces arranca un **temporizador de 0.25 s** (`window`). Si dentro de él la velocidad cae al menos 6 m/s (`minDrop`) **y** más deprisa que 48 m/s² (`decel`: tres veces lo que frenan los frenos, 16), el choque es violento y el RV pone **`damagedWindshield`** a true (`isWindshieldDamaged()`, `repairWindshield()` lo repara) y enseña el parabrisas roto en lugar del intacto (`setWindshieldModels`, dos `Part` que se alternan con `setPartVisible`). Parte de la caída es del golpe mismo (se comprueba al instante); un golpe lateral, de espaldas, contra el suelo, lento, contra algo ligero (Pingu) o solo frenar fuerte **no** lo rompen (probado sin ventana con 15 casos). Los modelos: `windshield.obj` (dos láminas translúcidas, como estaban en `rv.obj`) y `windshield_broken.obj` (dos cuadrados con la textura `windshield_cracked.png`, RGBA de 1024 × 512: telaraña de grietas radiales con ramas y anillos desde un punto de impacto delante del conductor, y el resto transparente con el tinte del cristal), todo de `generate_windshield.py`. El shader usa ahora el **alfa de la textura** para las mallas translúcidas (`outAlpha` en `shader.frag`); el material roto tiene `d 0.99` solo para ir en la pasada de translúcidos.
- **La cabina:** el salpicadero, la llave de contacto y las agujas de los relojes son **modelos aparte** que el RV añade como piezas suyas (`RV::setCockpitModels`, llamado desde `TestStage`), en el marco de `rv.obj`: `dashboard.obj` (`generate_dashboard.py`: panel de control, sobre una **cubierta elevada** en el lado del conductor (`RAISE` = 0.22 m, `SHIFT_Z` = 0.035) para que asome por debajo de la vista normal con la carretera encima, con dos relojes —velocidad y combustible—, pantalla digital, cerradura, interruptores y perillas, radio, rejillas, guantera y ranuras del desempañador; sin volante), `key.obj` (`generate_key.py`) y `needle.obj` (`generate_needle.py`: aguja roja con su cubo, una sola para los dos relojes). Dónde va cada uno lo escribe `generate_dashboard.py` en `dashboard_mounts.json` (origen y ejes del modelo en el mundo, y los ángulos de la aguja al principio y al final de la escala) y **`RV.cpp` repite esas constantes** (`PANEL_*`, `*_ORIGIN`, `*_ANGLES`): si se cambia el salpicadero, hay que copiarlas. Cada frame (`RV::updateCockpit`, en `update`): la **aguja de velocidad** sigue la velocidad hacia delante del RV (escala de 0 a 25 m/s, 270°), la de **combustible** muestra `fuel` (0–1, `setFuel`) y cae a vacío cuando nadie conduce (contacto apagado). **El motor (tecla R, `RV::setEngine/toggleEngine`)**: **el motor empieza apagado y subir al RV no lo arranca** (hay que pulsar R); bajar lo apaga. Con el motor apagado no hay empuje (como sin combustible: gira y rueda por inercia, y por debajo de 2 m/s el freno de mano lo sujeta), **las dos agujas caen a vacío sea cual sea la velocidad y el combustible reales**, **los faros se apagan** pero **el interruptor de luces conserva su estado** (`headlightsOn`; lo que luce es `lightsActive()` = interruptor y motor): al arrancar de nuevo vuelven a encenderse solos, con el brillo y la luz del salpicadero. F no hace nada mientras el motor está apagado y **la llave gira de vuelta** (la llave sigue a `engineOn`, ya no a `occupied`). **El combustible se gasta al conducir**, en proporción a la velocidad (`FUEL_PER_METER` = 1/1500 del depósito por metro recorrido: un depósito lleno da ~1.5 km; solo con el RV ocupado y en marcha; parado no gasta). **Sin combustible el motor no empuja**: `contactFloor` fuerza el acelerador a 0 (puede girar y rueda por inercia hasta pararse) y, por debajo de 2 m/s, pone el freno de mano para que no se escurra en una cuesta; no hay forma de repostar (solo `setFuel`), y la **llave gira** 40° en el sentido de las agujas del reloj al subir y vuelve al bajar. Todo con un poco de suavizado (6–8 /s) para que se mueva como lo real. Cada pieza es una `Part` con su `local` (`setPartTransform`). **Con los faros encendidos el salpicadero se ilumina** (`RV::updateDashboardLights`, desde `setHeadlights`): se muestra `dashboard_glow.obj` (una copia emisiva, `unlit` = 2, de las marcas de los relojes, los 6 pilotos de los interruptores y la ventana de la pantalla digital, generada por el mismo script, justo encima del salpicadero) y las agujas pasan a emisivas (`GameObject::setPartUnlit`). El cubo cromado de las agujas está en el salpicadero, no en `needle.obj`, para que no brille con ellas. Además esas partes **emiten luz**: `RV::getDashboardLights` añade (solo con los faros encendidos) cinco luces omnidireccionales pequeñas, **anaranjadas y tenues** (`DASH_LIGHT_COLOR` = 0.55, 0.25, 0.05), **una sobre cada componente que brilla** y con poco alcance (0.30–0.42 m): los dos relojes, la pantalla digital y cada columna de pilotos. Así el resplandor sale solo de ellos y tiñe de naranja lo que tienen al lado (el marco y los aros de los relojes, el cuerpo de los interruptores), sin bañar todo el panel. `TestStage::getSpotLights` las suma a los faros.
- **Faros:** con `Headlights` (F, solo conduciendo) `RV::toggleHeadlights()` enciende o apaga dos focos (`SpotLight`, `render/SpotLight.h`) en x = ±1.0, y = 0.99, en la parte delantera del RV, orientados hacia delante y un poco abajo (cono de ~21° a ~37°, 45 m de alcance). `RV::getHeadlights` los da en coordenadas de mundo (solo si están encendidos) y `main` los manda al shader cada frame. Las lentes de los 4 faros brillan con la pieza `headlight_glow.obj` (material emisivo `glow`, `unlit` = 2), que solo se dibuja encendidos (`GameObject::setPartVisible`). **Al bajar del RV con las luces encendidas, se quedan encendidas** (`parkedLights`: el motor se apaga pero el interruptor sigue dándoles corriente) hasta que alguien vuelva a subir; si el motor ya estaba apagado al bajar, siguen apagadas.
- **Bajar:** con la tecla `LeaveVehicle` (Mayús izquierda, reasignable), `TestStage::leaveVehicle()` pone al pingüino en el suelo junto a la puerta (`doorPosition(1.5)`, fuera de la caja del RV), con gravedad y colisión, y devuelve los controles y la cámara en primera persona, mirando hacia fuera de la puerta (`doorYaw()`). Un RV vacío tiene el **freno de mano** puesto (`VehicleBody::setHandbrake`), así que se queda donde se deja.
- **Cómo llega el cambio al bucle:** `GameStage::setPlayer` marca el cambio; el bucle principal lo recoge con `takePlayerChange()` y llama a `controller.attach(jugador, distancia, altura, yaw)`. `Controller::attach` acepta el rumbo inicial de la vista (por defecto, hacia −z).

## Satélite

`Satellite` representa una montura altazimutal, como una antena o un telescopio: un cubo (la cabeza) sobre un poste. La cara +y del cubo es el *boresight*, la dirección a la que apunta, y está marcada con una diana naranja (`assets/cube/`, generado por `generate_cube.py`).

- **Azimut:** de 0 a 360°, medido desde el norte (**−z**) en sentido horario hacia el este (**+x**).
- **Cénit:** de 0 a 90°. A 0 apunta al cielo en vertical y a 90 al horizonte. La elevación es 90 − cénit.
- **Dirección resultante:** `(sin c·sin a, cos c, −sin c·cos a)`. Se obtiene con la orientación `rotY(−a) · rotX(−c) · escala`.
- **Cómo gira:** las órdenes fijan un objetivo (`pointAt`, `setTargetAzimuth`, `setTargetZenith`, `stop`). `update(dt)` mueve los dos ejes a la vez, sin pasar de `slewRate` grados por segundo (entre 1 y 120). El azimut gira por el camino más corto.
- **Panel:** muestra el azimut y el cénit actuales, la elevación, el vector de dirección y el estado (girando o apuntando). Tiene deslizadores para el azimut y el cénit objetivo y para la velocidad, y botones Cenit, Norte 45 y Parar.
- **Dos objetos:** `Satellite` es la cabeza, que gira con su `rotation`, y su escala es el tamaño del cubo. El poste es un `GameObject` aparte (`getMount()`), porque en `GameObject` todas las piezas comparten la misma transformación. Hay que añadir ambos al stage.
- **Ubicación:** `TestStage` lo crea en (4.5, suelo, 2), con la altura que da `floorAt`, al alcance de quien esté en (3, suelo, 4), donde está el pingüino a pie. Con el RV como jugador hay que acercarlo: la distancia se mide desde su origen, en el centro de su base.

## Animación esquelética

1. `AnimatedModel::processAnimatedMesh` registra los huesos que tienen peso en una lista global (`pendingBones`). Asigna a cada vértice hasta 12 influencias y normaliza los pesos, porque el exportador los deja sumando entre 0.85 y 1.3.
2. `AnimatedModel::processNode` descarta las mallas que no tienen ni huesos ni textura (restos del exportador).
3. `Skeleton::Init` guarda los canales de la animación 0. Después, `Update(seconds)` interpola las claves (lerp/slerp en bucle), recorre los nodos y calcula `boneMats[i] = global(nodo del hueso) * offset`.
4. `computeFit` muestrea la animación 16 veces en la CPU para obtener la caja que ocupa el modelo y calcular `fitCenter`/`fitScale`.

## Ficheros de escena (`assets/scenes/*.scene`)

Son mapas descritos como datos. El juego los carga con `SceneStage` (`SceneFile` hace el parseo) y el visor web (`viewer.js`) muestra el mismo fichero, así que los dos parsers deben estar sincronizados. Los mapas hechos en código (`TestStage`) no se pueden ver en el visor.

Va un comando por línea, con los campos separados por espacios. `#` inicia un comentario. Las rutas de modelo son relativas a `assets/`.

| Comando | Argumentos | Notas |
|---|---|---|
| `moon` | `x y z` | Dirección hacia la luna (se normaliza). Debe coincidir con `MOON_DIR` de `assets/sky/generate_sky.py`, porque la luna está pintada en la textura del cielo. |
| `light` | `r g b` | Color de la luz. |
| `fog` | `r g b` | Color del horizonte: niebla y color de fondo. |
| `time_of_day` | `horas` | Hora al empezar (0 = medianoche). `Stage::setTimeOfDay`. El visor lo ignora. |
| `day_duration` | `segundos` | Segundos reales que dura un día entero; 0 = el tiempo está parado (por defecto). |
| `sky` | `modelo` | Cúpula de cielo (opcional). |
| `floor` | `modelo x y z` | Suelo por el que se anda (`Stage::setFloor`), también dibujado. Es la referencia de `ground`. |
| `player` | `modelo x y z` | Modelo animado del jugador (un `Walker`, con gravedad). `y` puede ser `ground`. Sin `player` hay un `Walker` invisible. |
| `camera` | `distancia altura` | Distancia detrás del jugador y altura sobre su posición, que está en sus pies. **Con distancia 0 es primera persona** y el jugador no se dibuja. En `desert.scene` vale `0 1.6`, la altura de los ojos del pingüino. |
| `object` | `modelo x y z yaw escala [efecto]` | Objeto estático. `yaw` en radianes. `y` es una altura del mundo o **`ground`**: sobre el suelo en (x, z), hundido 0.05 (`SceneStage::PROP_SINK`). Usa `ground` siempre que puedas, porque las dunas se regeneran. Efecto: `lit` (por defecto), `emissive` (pieza con `unlit` = 2) o `breathe` (`setBreathAmp(2)`). |

Si hay un error, el juego muestra `fichero:línea: mensaje` y no cambia de mapa (si es el primero, termina). El visor muestra el mismo mensaje en su barra de estado.

## Pipeline de assets

- `desert/`, `sky/` y `creature/` se generan con `python3 assets/<dir>/generate_*.py` (necesita numpy y Pillow). La salida es reproducible porque usan semilla. **No edites los OBJ ni los JPG a mano: cambia el script y regenera.**
- `music/` (`generate_desert_music.py`, solo numpy): **`desert.wav`**, la música de fondo del mapa de día (ver [Música de fondo](#música-de-fondo)). Es un bucle sin costura de 76.8 s (24 compases a 75 BPM), WAV de 16 bits, estéreo, a 32 kHz (unos 9.8 MB), que `AudioClip::loadWav` ya sabe leer. Árido, de aire flamenco/western en La con la cadencia andaluza (Am–G–F–E): guitarra clásica en arpegio y un banjo con la melodía (con trémolo en las notas largas), sintetizados con cuerda pulsada Karplus-Strong; un bordón grave en quinta, viento y un tambor de marco. Estructura: intro (compases 0–3), guitarra (4–11), guitarra + banjo + tambor (12–19) y final que vuelve a la intro (20–23). La reverberación es una convolución circular y el bordón y el viento tienen ciclos enteros en la duración del bucle, por eso no se oye el salto. Los niveles y las notas se ajustan con constantes al principio del script (`BPM`, `BANJO_GAIN`, `DRONE`, `MELODY`, `PROGRESSION`...). **`wind.wav`** (`generate_wind.py`): el viento de `desert.wav` solo, sin música. Sale de `build()` del script anterior, que mezcla la pieza y el viento por separado con el mismo ruido, reverb y nivel, así que es el mismo sonido y dura lo mismo (76.8 s, 9.8 MB, bucle sin costura). Es muy tenue (rms 0.005 frente a 0.084 de la mezcla): es lo único que queda de noche en el mapa de día.
- **El desierto** (`generate_assets.py`) genera tres mallas de terreno y carretera:
  - `dunes.obj`: solo las dunas (160 × 160 m), para el mapa de noche.
  - `dunes_loop.obj` y `road.obj`: las del mapa de día. La carretera es un circuito **cerrado** (`loop_point`, un anillo de 31 a 45 m del claro, con curvas de al menos 16 m de radio, que es lo que gira el RV) y **8 m de ancho** (`ROAD_WIDTH`, el RV mide 2.4). La clase `Track` guarda su línea central (un punto cada 0.5 m, dando la vuelta una vez) y la altura del lecho: la altura de las dunas bajo la línea, suavizada de forma circular (sin salto donde se cierra) para que la carretera ignore las dunas pequeñas. `Track.terrain` talla ese lecho en las dunas, con una rampa de 6 m a cada lado, y `make_road` tiende una cinta sobre él que se cierra sobre sí misma (la última fila es la primera y la textura se repite un número entero de veces).
  - El script imprime la longitud del circuito y la curva más cerrada, y falla si la carretera se doblaría sobre sí misma. Se cambia la forma en `loop_point` y se regenera.
  - El script también escribe `dunes_loop_materials.png`, el mapa de materiales (ver [Materiales del suelo](#materiales-del-suelo)); si se cambia `loop_point`, se regenera junto con el terreno y la carretera.
  - Las alturas de los objetos del mapa de día se piden al suelo en tiempo de ejecución (`groundAt`), ya no están escritas a mano; los `.scene` usan `ground`. Los 44 adornos de `TestStage` los reparte un script con semilla, a más de 7 m de la línea central y fuera del claro (14 dentro del anillo y 30 fuera). El visor muestra la `y` sugerida al pasar el cursor por las dunas.
- Las texturas se cargan con `stb_image` a partir del `map_Kd`/`map_Ks` del material, relativo al directorio del modelo. Assimp invierte las UV (`aiProcess_FlipUVs`).

## Recetas

**Añadir o mover un objeto.** En un mapa `.scene`, edita el fichero: con el visor abierto se recarga solo, y en el juego se ve al volver a cargar el mapa con Z. En `TestStage` (`test.cpp`), en su constructor: `auto o = make_shared<GameObject>(loadModel("../assets/..."))`, y después `setPosition`/`setYaw`/`setScale` y `add(o)`. Si se mueve solo, crea un `DynamicGameObject` y añádelo con `addDynamic`. Para apoyarlo en el suelo, usa `floorAt(x, z, altura)`.

**Añadir un modelo nuevo.** Copia el OBJ+MTL+textura en `assets/<algo>/` y cárgalo con `loadModel`. El stage lo carga una sola vez, aunque se use en varios objetos.

**Un personaje controlable nuevo.** Hereda de `PlayableCharacter` (ver `Walker` y `RV`): `control()` recibe la entrada, `update(dt)` la convierte en movimiento (con `steerTowards`) y `attachCamera`/`followCamera` deciden la cámara. Después, `controller.attach(personaje, distancia, altura)`.

**Añadir un material del suelo.** Añádelo al `enum class FloorMaterial` (antes de `Count`) y a `floorMaterialName` (y a `floorMaterialFromName` si una malla lo nombra); pinta su número en el mapa de materiales; y haz que lo tengan en cuenta quienes se comporten distinto sobre él (en el RV, `surfaceOf` en `RV.cpp`).

**Añadir un emisor de partículas.** Crea un `ParticleEmitter` con sus `ParticleSettings` (color, tamaño, gravedad, vida...), dáselo al stage con `addEmitter` (y guárdalo para moverlo) y, cada frame, ponle `setPosition`, `setDirection` y `setRate` (0 lo apaga). `main` ya dibuja todos los emisores del mapa. Para un efecto ligado a un objeto, mira `RV::updateDust`.

**Dar otra forma de colisión a un objeto.** Pásala como último argumento del constructor: `make_shared<GameObject>(modelo, make_shared<Box>(medioTamaño, centro))` (o una `Capsule(radio, alto, base)`). Sin ella, el objeto recibe una cápsula ajustada al modelo. Si un objeto no debe chocar con nada (el suelo, la carretera, decoración), llama a `setCollidable(false)` **antes** de `add()`.

**Un objeto que se mueve y choca.** Hereda de `DynamicGameObject` y añádelo con `addDynamic`. Si necesita reaccionar al choque con algo más que mover su posición (como el RV, que tiene un cuerpo de física propio), sobrescribe `applyCollision`; si necesita su propio suelo, sobrescribe `contactFloor` y devuelve `true`. Pon `setMass` si debe empujar o dejarse empujar de forma realista.

**Añadir un efecto de sombreado.** Impleméntalo en `shaders/animatedshader.vert` y/o `shaders/shader.frag`, controlado por un uniform. Fíjalo por objeto en `GameObject::Draw` (como `breathAmp`) o por pieza (como `Part::unlit`). Si el visor web tiene que mostrarlo, copia también el cambio en sus shaders (`viewer.js`).

**Cambiar la cámara del jugador.** `controller.attach(player, distancia, altura)` en `main`. Distancia 0 es primera persona (altura 1.6 = los ojos del pingüino). Con una distancia mayor que 0 es tercera persona y el pingüino vuelve a verse, aunque medio hundido: `AnimatedModel` lo centra en su posición y la posición está en los pies.

## Limitaciones conocidas

- La interfaz muestra el texto en ASCII (`toAscii` quita tildes y eñes). Recibe teclas sueltas (`onKey`), pero no hay campos de texto ni rueda del ratón.
- Solo se reproduce la primera animación del FBX y no hay mezcla entre animaciones (`Animation` está sin usar).
- Las colisiones entre objetos no tienen rebote ni rozamiento, solo se separan. Los objetos estáticos se colocan en la rejilla al hacer `add()` y no pueden moverse después. Nadie puede saltar (no hay tecla para ello).
- Un RV volcado vuelve solo a apoyarse en las ruedas (autoenderezado), pero no hay un botón de recolocarlo.
- El visor web solo muestra los mapas `.scene` (la noche), no `TestStage`.
- Al cambiar de mapa se vuelven a cargar todos sus modelos (no hay caché entre mapas).
- El volumen no se puede ajustar en Opciones (existe `SoundEngine::setMasterVolume`). La voz de espeak-ng es robótica.
- Los subtítulos avanzan en proporción al tiempo de audio, no palabra a palabra.
- Las partículas son discos lisos: sin textura, sin iluminación y sin niebla (el polvo lejano no se funde con el horizonte).
- Nunca se liberan los recursos GL (VAO/VBO/texturas). Las texturas no se comparten entre modelos distintos.
- `Model` ignora las transformaciones de los nodos del fichero. Si un OBJ/FBX estático depende de ellas, aparecerá mal colocado.
