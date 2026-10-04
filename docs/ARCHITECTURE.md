# Arquitectura del motor

Este documento explica cómo está montado el motor 3D y cómo se dibuja un frame. También indica qué hay que tocar para las tareas más habituales. Para compilar y ejecutar, consulta el [README](../README.md).

## Mapa del repositorio

```
src/                 motor + juego (C++11, OpenGL 3.3 core)
  test.cpp           main: ventana, carga de la escena y bucle principal
  *.h / *.cpp        una clase por par de ficheros (ver tabla de abajo)
  animatedshader.vert, shader.frag   el único programa de shaders en uso
  makefile           compila todos los .cpp de src/ y genera ../test/test
assets/
  scenes/*.scene     disposición de cada escena (datos, ver más abajo)
  desert/ sky/ creature/   assets procedurales + su script generate_*.py
  ping/              pingüino animado (FBX)
  backpack/          modelo de ejemplo (sin usar)
tools/scene_viewer/  visor web de escenas (ver su README)
docs/                esta documentación
```

## Módulos

```
            test.cpp (main)
         ┌─────┼─────────┬──────────┐
     Controller  Scene     Light     Shader
         │    ┌───┴──────────┐
       Camera  SceneFile  GameObject ── Model ── Mesh
                              │
                        AnimatedModel ── AnimatedMesh
                              │
                           Skeleton
           (todo usa myopengl: utilidades GL, texturas, conversión de matrices)
```

| Clase | Responsabilidad |
|---|---|
| `Shader` | Compila y enlaza vert+frag desde ficheros y expone setters de uniforms. Solo hay uno y queda activo tras crearse. |
| `Mesh` | Geometría estática en GPU (VAO/VBO/EBO) y sus texturas. |
| `Model` | Carga un fichero con Assimp y crea un `Mesh` por cada `aiMesh`. Ignora las transformaciones de los nodos. |
| `AnimatedMesh` | Como `Mesh`, pero con 12 pares (hueso, peso) por vértice. |
| `AnimatedModel` | Carga un FBX con esqueleto: mallas, lista global de huesos, `Skeleton` y ajuste de escala (`computeFit`). Posee el `Assimp::Importer`. |
| `Skeleton` | Evalúa la primera animación del fichero y calcula las matrices de hueso (`boneMats`, como máximo 100). |
| `GameObject` | Instancia de un modelo (estático o animado) con posición y matriz de rotación/escala. No es dueño del modelo. `Update(dt)` y `Draw()` son virtuales para que las subclases añadan comportamiento. |
| `Satellite` | `GameObject` + `Interactable`: cubo orientable en azimut y cénit sobre un poste. Ver [Satélite](#satélite). |
| `SceneFile` | Parser de ficheros `.scene`. No depende de OpenGL. |
| `Scene` | Construye la escena a partir de un `SceneFile`. Carga cada modelo una sola vez, crea los `GameObject` y los dibuja con su efecto. |
| `Camera` | Calcula las matrices de proyección y vista y sigue a un `GameObject`: en primera persona (distancia 0, a la altura de los ojos) o en tercera, desde detrás. |
| `Controller` | Gestiona la entrada: el ratón mueve la cámara y WASD/Espacio/Shift mueven al personaje. Se puede pausar (`setEnabled(false)`) mientras hay una interfaz abierta. |
| `Interactable` | Interfaz (clase abstracta) de los objetos que el jugador puede usar: nombre, punto, alcance y `buildInterface(UIPanel&)`. |
| `InteractionSystem` | Busca el `Interactable` más cercano al jugador, muestra el aviso y abre/cierra su panel con E/Esc, pausando el `Controller`. |
| `UIManager`, `UIRenderer`, `UI*` | Sistema de interfaz 2D genérico. Ver [Interfaz de usuario](#interfaz-de-usuario-ui). |
| `Light` | La única luz puntual del shader. |
| `Animation` | Rango de frames con nombre. **Todavía no se usa.** |

## Arranque

`main` cambia el directorio de trabajo a `src/`, que localiza junto al ejecutable a partir de `/proc/self/exe` (`test/test` → `test/../src`). Por eso los shaders se abren como `animatedshader.vert` y los assets como `../assets/...`, se lance desde donde se lance. Si falta un shader, `fileToString` lo dice y termina. Antes devolvía una cadena vacía y el driver acababa fallando al enlazar con un error confuso (`must write to gl_Position`).

## Un frame

```
camera.resize()           viewport y proyección si cambia el framebuffer
interaction.update(pos)   aviso "E: usar ..."; E/Esc abren o cierran el panel y pausan el Controller
ui.update()               ratón → paneles (pulsar, arrastrar, soltar)
controller.update()       ratón → rotación de la cámara; teclas → mueve al jugador; camera.follow()
glClear(fogColor)
scene.Update(t)           avanza la animación del jugador (Skeleton)
scene.Draw(shader, camPos, t)
  1. cielo: centrado en la cámara, sin depth test, unlit = 1
  2. objetos: unlit / breathAmp según su efecto
  3. jugador: skinned = 1, gBones, fitCenter/fitScale
satellite.Update(dt) / Draw()
ui.draw()                 interfaz 2D encima de todo
glfwSwapBuffers / glfwPollEvents
```

`dt` (segundos desde el frame anterior) solo lo usan los objetos con `Update(dt)`, como el satélite. El movimiento del jugador sigue avanzando una cantidad fija por frame (con vsync activo).

## Sistema de coordenadas y transformaciones

- Mano derecha, **+y arriba**. Al empezar, la cámara mira hacia **−z**. El suelo del desierto está en y = −1. El pingüino aparece en el origen con los pies en y ≈ −0.9.
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
| `breathAmp`, `breathTime` | `Scene::Draw` | Respiración procedural de mallas estáticas (efecto `breathe`). 0 la desactiva. |
| `unlit` | `Scene::Draw` | 0 = iluminado (Phong + niebla), 1 = cúpula de cielo (estrellas que titilan), 2 = emisivo. |
| `lightPosition`, `lightColor` | `Light` | Luz de la luna, situada a 100 unidades en la dirección `moon`. |
| `moonDir`, `fogColor`, `time` | `Scene::Draw` | Cielo y niebla. La niebla va de 30 a 70 unidades de distancia. |
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
3. Regístralo con `interaction.add(&objeto)` en `test.cpp`.

## Satélite

`Satellite` representa una montura altazimutal, como una antena o un telescopio: un cubo (la cabeza) sobre un poste. La cara +y del cubo es el *boresight*, la dirección a la que apunta, y está marcada con una diana naranja (`assets/cube/`, generado por `generate_cube.py`).

- **Azimut:** de 0 a 360°, medido desde el norte (**−z**) en sentido horario hacia el este (**+x**).
- **Cénit:** de 0 a 90°. A 0 apunta al cielo en vertical y a 90 al horizonte. La elevación es 90 − cénit.
- **Dirección resultante:** `(sin c·sin a, cos c, −sin c·cos a)`. Se obtiene con la orientación `rotY(−a) · rotX(−c) · escala`.
- **Cómo gira:** las órdenes fijan un objetivo (`pointAt`, `setTargetAzimuth`, `setTargetZenith`, `stop`). `Update(dt)` mueve los dos ejes a la vez, sin pasar de `slewRate` grados por segundo (entre 1 y 120). El azimut gira por el camino más corto.
- **Panel:** muestra el azimut y el cénit actuales, la elevación, el vector de dirección y el estado (girando o apuntando). Tiene deslizadores para el azimut y el cénit objetivo y para la velocidad, y botones Cenit, Norte 45 y Parar.
- **Ubicación:** se crea en `test.cpp`, en (−2, −1, −2), al alcance del jugador desde el punto de inicio. No está en el `.scene` y por eso el visor web no lo muestra.

## Animación esquelética

1. `AnimatedModel::processAnimatedMesh` registra los huesos que tienen peso en una lista global (`pendingBones`). Asigna a cada vértice hasta 12 influencias y normaliza los pesos, porque el exportador los deja sumando entre 0.85 y 1.3.
2. `AnimatedModel::processNode` descarta las mallas que no tienen ni huesos ni textura (restos del exportador).
3. `Skeleton::Init` guarda los canales de la animación 0. Después, `Update(seconds)` interpola las claves (lerp/slerp en bucle), recorre los nodos y calcula `boneMats[i] = global(nodo del hueso) * offset`.
4. `computeFit` muestrea la animación 16 veces en la CPU para obtener la caja que ocupa el modelo y calcular `fitCenter`/`fitScale`.

## Ficheros de escena (`assets/scenes/*.scene`)

Describen qué hay en el mundo y dónde está. Los leen **el juego** (`SceneFile`) y **el visor** (`viewer.js`), así que la escena tiene una única fuente de verdad. El juego carga por defecto `assets/scenes/desert.scene`, y se le puede pasar otra escena como argumento (ruta relativa al directorio desde el que se lanza): `./test/test --windowed mi.scene`.

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

**Añadir o mover un objeto.** Edita el `.scene`. Con el visor abierto (`python3 tools/scene_viewer/serve.py`), la escena se recarga sola al guardar. Para saber la altura del suelo, pasa el cursor por las dunas en el visor.

**Añadir un modelo nuevo.** Copia el OBJ+MTL+textura en `assets/<algo>/` y referéncialo con un `object`. `Scene` lo carga una sola vez, aunque se use en varios objetos.

**Añadir un efecto de sombreado.**
1. Añade un valor a `Effect` y su nombre en `parseEffect` (`SceneFile.cpp`).
2. Decide en `Scene::Draw` qué uniforms activa.
3. Implementa el efecto en `animatedshader.vert` y/o `shader.frag`.
4. Replica los pasos 1 a 3 en `viewer.js`: la lista de efectos de `parseScene`, `MODES`/`makeMaterial` y los shaders.

**Cambiar el personaje.** Cambia el `player` del `.scene`. Debe ser un FBX con una animación. Se reescala solo a ~1.8 de alto. Si al andar no mira hacia donde camina, ajusta `MODEL_FORWARD_OFFSET` en `Controller.cpp`.

## Limitaciones conocidas

- La interfaz solo muestra texto ASCII (stb_easy_font) y no tiene entrada de teclado (campos de texto) ni rueda del ratón.
- Solo se reproduce la primera animación del FBX y no hay mezcla entre animaciones (`Animation` está sin usar).
- No hay delta time. Tampoco hay colisiones ni seguimiento del terreno: el jugador se mueve a altura fija salvo con Espacio/Shift.
- Nunca se liberan los recursos GL (VAO/VBO/texturas). Las texturas no se comparten entre modelos distintos.
- `Model` ignora las transformaciones de los nodos del fichero. Si un OBJ/FBX estático depende de ellas, aparecerá mal colocado.
