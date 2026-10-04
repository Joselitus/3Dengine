# Arquitectura del motor

Este documento explica cómo está montado el motor 3D y cómo se dibuja un frame. También indica qué hay que tocar para las tareas más habituales. Para compilar y ejecutar, consulta el [README](../README.md).

## Mapa del repositorio

```
src/                 motor + juego (C++11, OpenGL 3.3 core)
  test.cpp           main + TestStage (el contenido del nivel) y bucle principal
  *.h / *.cpp        una clase por par de ficheros (ver tabla de abajo)
  animatedshader.vert, shader.frag   el único programa de shaders en uso
  makefile           compila todos los .cpp de src/ y genera ../test/test
assets/
  scenes/*.scene     escenas en datos; hoy solo las usa el visor (ver más abajo)
  desert/ sky/ creature/ rv/ cube/   assets procedurales + su script generate_*.py
  ping/              pingüino animado (FBX), el jugador
  backpack/          modelo de ejemplo (sin usar)
tools/scene_viewer/  visor web de escenas (ver su README)
docs/                esta documentación
```

## Módulos

```
                     test.cpp (main)
      ┌────────────┬──────┴───────┬──────────────┬──────────┐
  Controller   TestStage : Stage  UIManager  InteractionSystem  Light, Shader
      │             │                 │              │
    Camera      GameObject ─ Model    UI*       Interactable
                    ├── Satellite (+ Interactable)
                    └── DynamicGameObject
                          └── PlayableCharacter (abstracta)
                                ├── Walker  (el jugador: a pie, 1ª persona)
                                └── RV      (autocaravana conducible, aparcada)
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
| `GameObject` | Todo lo que está en el mundo. Hecho de piezas (`Part`: un `shared_ptr<Model>` y su modo `unlit`) o de un `AnimatedModel`, con posición, rotación y escala. `update(dt)` es virtual. `Draw()` fija los uniforms y no dibuja nada si `setVisible(false)`. |
| `DynamicGameObject` | `GameObject` que se mueve: velocidad, aceleración, velocidad máxima y gravedad. `steerTowards` acelera hacia una velocidad deseada. |
| `PlayableCharacter` | Personaje que maneja el `Controller` (abstracta). Cada uno decide cómo responde a la entrada (`control`) y cómo lo sigue la cámara (`attachCamera`, `followCamera`). |
| `Walker` | El jugador: anda en la dirección de la cámara y se orienta hacia donde camina. Con distancia de cámara 0 es primera persona y se oculta a sí mismo. |
| `RV` | Autocaravana: W/S aceleran y A/D giran, como un coche. Ahora está aparcada (no es el jugador). |
| `Satellite` | `GameObject` + `Interactable`: cubo orientable en azimut y cénit. Ver [Satélite](#satélite). |
| `Stage` | Nivel (abstracta): es dueño de los objetos estáticos y dinámicos, carga cada modelo una sola vez y tiene el suelo (height field o rayo hacia abajo, `floorAt`). Cada frame actualiza los objetos y aplica `apply()` a los dinámicos. |
| `TestStage` (`test.cpp`) | El desierto: dunas, carretera, cactus, rocas, la autocaravana, el satélite y el jugador. Mantiene a los dinámicos sobre el suelo (`collideWithFloor`). |
| `Camera` | Calcula las matrices de proyección y vista y sigue a un `GameObject`: en primera persona (distancia 0, a la altura de los ojos) o en tercera, desde detrás. |
| `Controller` | Gestiona la entrada: el ratón mueve la cámara y WASD/Espacio/Shift llegan al `PlayableCharacter`. Se puede pausar (`setEnabled(false)`) mientras hay una interfaz abierta; al pausar, el personaje recibe una entrada nula. |
| `Interactable` | Interfaz (clase abstracta) de los objetos que el jugador puede usar: nombre, punto, alcance y `buildInterface(UIPanel&)`. |
| `InteractionSystem` | Busca el `Interactable` más cercano al jugador, muestra el aviso y abre/cierra su panel con E/Esc, pausando el `Controller`. |
| `UIManager`, `UIRenderer`, `UI*` | Sistema de interfaz 2D genérico. Ver [Interfaz de usuario](#interfaz-de-usuario-ui). |
| `Light` | La única luz puntual del shader (el sol). |
| `SceneFile`, `Scene` | Escena descrita en un `.scene`. **El juego ya no los usa** (lo sustituye `Stage`); siguen compilando y el visor web lee el mismo formato. |
| `Animation` | Rango de frames con nombre. **Todavía no se usa.** |

## Arranque

`main` cambia el directorio de trabajo a `src/`, que localiza junto al ejecutable a partir de `/proc/self/exe` (`test/test` → `test/../src`). Por eso los shaders se abren como `animatedshader.vert` y los assets como `../assets/...`, se lance desde donde se lance. Si falta un shader, `fileToString` lo dice y termina. Antes devolvía una cadena vacía y el driver acababa fallando al enlazar con un error confuso (`must write to gl_Position`).

## Un frame

```
camera.resize()              viewport y proyección si cambia el framebuffer
interaction.update(pos)      aviso "E: usar ..."; E/Esc abren o cierran el panel y pausan el Controller
ui.update()                  ratón → paneles (pulsar, arrastrar, soltar)
controller.update()          ratón → rotación de la cámara; teclas → player->control(dir, up, yaw)
stage.update(dt)             update(dt) de cada objeto; a los dinámicos, además, apply() (suelo)
player->followCamera()       la cámara sigue al jugador ya movido
glClear(horizonte)
stage.Draw(shader, t)        cada GameObject fija objposition/objrotation/unlit/breathAmp y se dibuja
ui.draw()                    interfaz 2D encima de todo
glfwSwapBuffers / glfwPollEvents
```

`dt` son los segundos desde el frame anterior: el movimiento no depende de los FPS.

## Sistema de coordenadas y transformaciones

- Mano derecha, **+y arriba**. Al empezar, la cámara mira hacia **−z**. El claro del desierto está en y = −1 (`GROUND_Y`), y fuera de él la altura la da el suelo del `Stage`. La posición del jugador está en sus pies, sobre el suelo, y la cámara queda 1.6 por encima.
- La rotación positiva alrededor de +y (yaw) es antihoraria vista desde arriba (`glm::rotate`).
- En el vertex shader: `world = objrotation * fitted + objposition` y `gl_Position = projection * model * view * world`.
  - **`model` no es la matriz del objeto.** Es la rotación de la cámara (pitch·yaw), que se aplica *después* de `view` (la traslación a la posición de la cámara). Así la cámara gira sobre sí misma.
  - `objrotation` contiene la rotación y también la escala del objeto. Las normales se transforman con su `mat3` y se normalizan en el fragment shader.

## Contrato del shader

El motor dibuja todo con **`animatedshader.vert` + `shader.frag`**. `shader.vert` es antiguo y ya no se usa.

| Uniform | Lo escribe | Significado |
|---|---|---|
| `projection`, `view`, `model`, `viewPosition` | `Camera::update` | Cámara (ver arriba). |
| `objposition`, `objrotation` | `GameObject::Draw` | Transformación del objeto. |
| `skinned` | `GameObject` (0) / `AnimatedModel` (1) | Activa el skinning. |
| `gBones[100]`, `meshMat` | `AnimatedModel::Draw` | Matrices de hueso. `meshMat` se usa con mallas animadas que no tienen pesos (siguen a su nodo). |
| `fitCenter`, `fitScale` | `AnimatedModel::Draw` | Normaliza el modelo animado a ~1.8 de alto alrededor del origen. |
| `breathAmp` / `breathTime` | `GameObject::Draw` / `Stage::Draw` | Respiración procedural de mallas estáticas. 0 la desactiva. |
| `unlit` | `GameObject::Draw` (por pieza) | 0 = iluminado (Phong + niebla), 1 = cúpula de cielo (estrellas que titilan), 2 = emisivo. |
| `lightPosition`, `lightColor` | `Light` | El sol, a 100 unidades en la dirección `sunDir` (`test.cpp`). |
| `moonDir`, `fogColor`, `time` | `test.cpp` | Dirección del astro y color del horizonte, que es también el de la niebla (de 30 a 70 unidades). |
| `texture_diffuse1` (…) | `Mesh` / `AnimatedMesh::Draw` | Texturas del material. |

Atributos: 0 posición, 1 normal, 2 uv, 3–8 tres grupos `ivec4` de ids de hueso + `vec4` de pesos.

**El visor web contiene una copia de estos shaders** (`tools/scene_viewer/viewer.js`). Si cambias la iluminación, la niebla, el cielo o la respiración, cambia también la copia.

## Interfaz de usuario (UI)

Es un sistema propio y orientado a objetos (sin dependencias externas), pensado para que cualquier objeto del juego ofrezca su propio panel.

```
UIElement (abstracta)            colocación (layout), dibujo (draw) y ratón
├── UILabel                      texto fijo o generado cada frame (valores en vivo)
├── UIButton                     acción al pulsar y soltar encima
├── UISlider                     número en [min, max]; lee y escribe a través de funciones
└── UIContainer (abstracta)      posee a sus hijos
    ├── UIPanel                  ventana: título, botón de cerrar, arrastrable; hijos en vertical
    └── UIRow                    hijos en horizontal, repartiendo el ancho

UIManager    paneles abiertos, reparto del ratón, aviso inferior, dibujo
UIRenderer   rectángulos y texto en píxeles de ventana, en un solo draw call
Interactable contrato entre un objeto del juego y la interfaz
```

- **Coordenadas:** píxeles de ventana con (0, 0) arriba a la izquierda, las mismas que `glfwGetCursorPos`.
- **Ratón:** `UIManager::update` busca el elemento bajo el cursor (`elementAt`, recursivo). Si es interactivo, recibe `onPress`, luego `onDrag` mientras se mantiene el botón y por último `onRelease`. Si no lo es (etiquetas, filas), el clic va a su panel, que se encarga del arrastre por el título y del botón de cerrar. El panel pulsado pasa al frente.
- **Valores en vivo:** `UILabel` y `UISlider` no guardan el valor, lo leen cada frame con una `std::function`. Así siempre muestran el estado real del objeto, aunque cambie por otra vía (por ejemplo, mientras el satélite gira).
- **Dibujo:** `UIRenderer` usa su propio shader (`ui.vert`/`ui.frag`) y, al terminar, **vuelve a activar el programa anterior**, porque los setters de `Shader` suponen que el shader del motor está activo. Desactiva el depth test y activa el blending solo mientras dibuja.
- **Texto:** se dibuja con `stb_easy_font.h` (de dominio público y en `src/`). **Solo admite ASCII**, así que los textos de la interfaz van sin tildes ni símbolos como °.
- **Estilo:** los colores y márgenes son comunes y están en `UITheme` (`UIElement.h`).

**Hacer que un objeto se pueda usar:**
1. Hereda de `Interactable` e implementa `getInteractionName()`, `getInteractionPoint()` y `buildInterface(UIPanel&)` (opcionalmente también `getInteractionRange()`, que por defecto es 3).
2. En `buildInterface`, añade elementos con `panel.add(new UILabel(...))` y similares. Las lambdas pueden capturar `this`, siempre que el objeto viva más que el panel.
3. Créalo en el `Stage` (en `TestStage`: `add(objeto)` y `interactables.push_back(objeto.get())`). `main` registra todos los de `getInteractables()` en el `InteractionSystem`.

## Satélite

`Satellite` representa una montura altazimutal, como una antena o un telescopio: un cubo (la cabeza) sobre un poste. La cara +y del cubo es el *boresight*, la dirección a la que apunta, y está marcada con una diana naranja (`assets/cube/`, generado por `generate_cube.py`).

- **Azimut:** de 0 a 360°, medido desde el norte (**−z**) en sentido horario hacia el este (**+x**).
- **Cénit:** de 0 a 90°. A 0 apunta al cielo en vertical y a 90 al horizonte. La elevación es 90 − cénit.
- **Dirección resultante:** `(sin c·sin a, cos c, −sin c·cos a)`. Se obtiene con la orientación `rotY(−a) · rotX(−c) · escala`.
- **Cómo gira:** las órdenes fijan un objetivo (`pointAt`, `setTargetAzimuth`, `setTargetZenith`, `stop`). `update(dt)` mueve los dos ejes a la vez, sin pasar de `slewRate` grados por segundo (entre 1 y 120). El azimut gira por el camino más corto.
- **Panel:** muestra el azimut y el cénit actuales, la elevación, el vector de dirección y el estado (girando o apuntando). Tiene deslizadores para el azimut y el cénit objetivo y para la velocidad, y botones Cenit, Norte 45 y Parar.
- **Dos objetos:** `Satellite` es la cabeza, que gira con su `rotation`, y su escala es el tamaño del cubo. El poste es un `GameObject` aparte (`getMount()`), porque en `GameObject` todas las piezas comparten la misma transformación. Hay que añadir ambos al stage.
- **Ubicación:** `TestStage` lo crea en (4.5, suelo, 2), con la altura que da `floorAt`, al alcance del jugador desde el punto de inicio (3, suelo, 4).

## Animación esquelética

1. `AnimatedModel::processAnimatedMesh` registra los huesos que tienen peso en una lista global (`pendingBones`). Asigna a cada vértice hasta 12 influencias y normaliza los pesos, porque el exportador los deja sumando entre 0.85 y 1.3.
2. `AnimatedModel::processNode` descarta las mallas que no tienen ni huesos ni textura (restos del exportador).
3. `Skeleton::Init` guarda los canales de la animación 0. Después, `Update(seconds)` interpola las claves (lerp/slerp en bucle), recorre los nodos y calcula `boneMats[i] = global(nodo del hueso) * offset`.
4. `computeFit` muestrea la animación 16 veces en la CPU para obtener la caja que ocupa el modelo y calcular `fitCenter`/`fitScale`.

## Ficheros de escena (`assets/scenes/*.scene`)

> **Desfasado tras el merge con `main`.** El juego monta el nivel en código (`TestStage` en `test.cpp`) y ya no lee los `.scene`. `desert.scene` describe el desierto nocturno anterior, así que el visor web muestra esa escena y no la actual. Falta decidir si `Stage` pasa a cargar `.scene` o si se retiran el formato y el visor.

Describen qué hay en el mundo y dónde está, y los lee el visor (`viewer.js`). `SceneFile`/`Scene` también saben cargarlos.

Va un comando por línea, con los campos separados por espacios. `#` inicia un comentario. Las rutas de modelo son relativas a `assets/`.

| Comando | Argumentos | Notas |
|---|---|---|
| `moon` | `x y z` | Dirección hacia la luna (se normaliza). Debe coincidir con `MOON_DIR` de `assets/sky/generate_sky.py`, porque la luna está pintada en la textura del cielo. |
| `light` | `r g b` | Color de la luz. |
| `fog` | `r g b` | Color del horizonte: niebla y color de fondo. |
| `sky` | `modelo` | Cúpula de cielo (opcional). |
| `player` | `modelo x y z` | Modelo animado que maneja el `Controller` (opcional). |
| `camera` | `distancia altura` | Distancia detrás del jugador y altura sobre su origen. **Con distancia 0 es primera persona:** la cámara queda en los ojos y el jugador no se dibuja (`Scene::setPlayerVisible`). En `desert.scene` vale `0 0.7`: el pingüino mide 1.8 y está centrado en su origen, así que la coronilla queda en +0.9. |
| `object` | `modelo x y z yaw escala [efecto]` | Objeto estático. `yaw` en radianes. `y` es la altura final en el mundo. Efecto: `lit` (por defecto), `emissive` o `breathe`. |

Si hay un error, el juego muestra `fichero:línea: mensaje` y termina. El visor muestra el mismo mensaje en su barra de estado.

## Pipeline de assets

- `desert/`, `sky/` y `creature/` se generan con `python3 assets/<dir>/generate_*.py` (necesita numpy y Pillow). La salida es reproducible porque usan semilla. **No edites los OBJ ni los JPG a mano: cambia el script y regenera.**
- Las alturas `y` de los objetos del desierto salen de `dune_height(x, z)` en `generate_assets.py`: `y = -1 + dune_height(x, z) - 0.05`. El visor muestra la `y` sugerida al pasar el cursor por las dunas.
- Las texturas se cargan con `stb_image` a partir del `map_Kd`/`map_Ks` del material, relativo al directorio del modelo. Assimp invierte las UV (`aiProcess_FlipUVs`).

## Recetas

**Añadir o mover un objeto.** En el constructor de `TestStage` (`test.cpp`): `auto o = make_shared<GameObject>(loadModel("../assets/..."))`, y después `setPosition`/`setYaw`/`setScale` y `add(o)`. Si se mueve solo, crea un `DynamicGameObject` y añádelo con `addDynamic`. Para apoyarlo en el suelo, usa `floorAt(x, z, altura)`.

**Añadir un modelo nuevo.** Copia el OBJ+MTL+textura en `assets/<algo>/` y cárgalo con `loadModel`. El stage lo carga una sola vez, aunque se use en varios objetos.

**Un personaje controlable nuevo.** Hereda de `PlayableCharacter` (ver `Walker` y `RV`): `control()` recibe la entrada, `update(dt)` la convierte en movimiento (con `steerTowards`) y `attachCamera`/`followCamera` deciden la cámara. Después, `controller.attach(personaje, distancia, altura)`.

**Añadir un efecto de sombreado.** Impleméntalo en `animatedshader.vert` y/o `shader.frag`, controlado por un uniform. Fíjalo por objeto en `GameObject::Draw` (como `breathAmp`) o por pieza (como `Part::unlit`). Si el visor web tiene que mostrarlo, copia también el cambio en sus shaders (`viewer.js`).

**Cambiar la cámara del jugador.** `controller.attach(player, distancia, altura)` en `main`. Distancia 0 es primera persona (altura 1.6 = los ojos del pingüino). Con una distancia mayor que 0 es tercera persona y el pingüino vuelve a verse, aunque medio hundido: `AnimatedModel` lo centra en su posición y la posición está en los pies.

## Limitaciones conocidas

- La interfaz solo muestra texto ASCII (stb_easy_font) y no tiene entrada de teclado (campos de texto) ni rueda del ratón.
- Solo se reproduce la primera animación del FBX y no hay mezcla entre animaciones (`Animation` está sin usar).
- No hay colisiones entre objetos: solo con el suelo. Con gravedad, Espacio/Shift no hacen nada (no se puede saltar).
- El visor web y `desert.scene` no reflejan el nivel actual (ver [Ficheros de escena](#ficheros-de-escena-assetsscenesscene)).
- Nunca se liberan los recursos GL (VAO/VBO/texturas). Las texturas no se comparten entre modelos distintos.
- `Model` ignora las transformaciones de los nodos del fichero. Si un OBJ/FBX estático depende de ellas, aparecerá mal colocado.
