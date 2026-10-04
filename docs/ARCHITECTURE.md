# Arquitectura del motor

Este documento explica cómo está montado el motor 3D y cómo se dibuja un frame. También indica qué hay que tocar para las tareas más habituales. Para compilar y ejecutar, consulta el [README](../README.md).

## Mapa del repositorio

```
src/                 motor + juego (C++11, OpenGL 3.3 core)
  test.cpp           main: lista de mapas, cambio de mapa, TestStage (el mapa de día) y bucle principal
  *.h / *.cpp        una clase por par de ficheros (ver tabla de abajo)
  animatedshader.vert, shader.frag   el único programa de shaders en uso
  makefile           compila todos los .cpp de src/ y genera ../test/test
assets/
  scenes/*.scene     mapas en datos (SceneStage), también los muestra el visor
  desert/ sky/ creature/ rv/ cube/   assets procedurales + su script generate_*.py
  ping/              pingüino animado (FBX), el jugador
  backpack/          modelo de ejemplo (sin usar)
tools/scene_viewer/  visor web de escenas (ver su README)
docs/                esta documentación
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

   GameObject ─ Model
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
| `GameStage` | Mapa jugable (abstracta, hereda de `Stage`): añade todo lo que el juego necesita para ejecutarlo y cambiarlo en marcha. Incluye el `Environment` (dirección y color de la luz, color del horizonte), el cielo opcional (`setSky`) y el jugador con la cámara que quiere (distancia, altura). También tiene los interactuables, una regla `apply()` por defecto (suelo) y `render()` (cielo alrededor de la cámara + stage). |
| `TestStage` (`test.cpp`) | Mapa "Desierto de dia", montado en código (de `main`): dunas, carretera, cactus, rocas, la autocaravana, el satélite y el jugador, con luz de sol. |
| `SceneStage` | Mapa a partir de un `.scene` ("Desierto de noche" = `desert.scene`): suelo, objetos (apoyados con `ground`), efectos, cielo, luz y un `Walker` como jugador. |
| `MapSelector` | Menú de depuración (tecla Z) para cambiar de mapa (subclase de `UIPanel`). Ver [Mapas](#mapas-y-selector-de-depuración). |
| `Camera` | Calcula las matrices de proyección y vista y sigue a un `GameObject`: en primera persona (distancia 0, a la altura de los ojos) o en tercera, desde detrás. El FOV (`setFov`) y la sensibilidad (`setSensitivity`) se pueden cambiar en marcha. |
| `Controller` | Gestiona la entrada: el desplazamiento del ratón en cada frame × la sensibilidad gira la cámara, y WASD/Espacio/Shift llegan al `PlayableCharacter`. Se puede pausar (`setEnabled(false)`), y entonces el personaje recibe una entrada nula. **No lee Esc.** |
| `PauseMenu`, `OptionsMenu`, `ControlsMenu` | Menús del juego (subclases de `UIPanel`) que reciben un `MenuContext`. Ver [Menús](#menús-pausa-y-opciones). |
| `Controls` | Registro de teclas: qué tecla hace cada `Action`. Todo lo que lee teclado lo consulta aquí. Ver [Controles](#controles-y-teclas). |
| `Interactable` | Interfaz (clase abstracta) de los objetos que el jugador puede usar: nombre, punto, alcance y `buildInterface(UIPanel&)`. |
| `InteractionSystem` | Busca el `Interactable` más cercano al jugador, muestra el aviso y abre o cierra su panel con E. Solo abre si no hay otro panel abierto. Esc lo cierra `UIManager`. |
| `UIManager`, `UIRenderer`, `UI*` | Sistema de interfaz 2D genérico. Ver [Interfaz de usuario](#interfaz-de-usuario-ui). |
| `Light` | La única luz puntual del shader (el sol o la luna, según el mapa). |
| `SoundEngine`, `Sound`, `AudioClip` | Motor de sonido (miniaudio): reproduce clips en memoria, en 3D o directos; el oyente sigue a la cámara. Ver [Audio y voz](#audio-y-voz-tts). |
| `SpeechSynthesizer`, `EspeakSynthesizer`, `Voice` | Texto a voz: la interfaz abstracta, su implementación con espeak-ng, y una voz que dice textos en segundo plano e informa del progreso. |
| `Npc` | `DynamicGameObject` + `Interactable`: personaje con un `Dialogue` que dice su `Voice`. Al usarlo se gira hacia el jugador y habla, con subtítulos sincronizados. |
| `Dialogue`, `LineNarrator`, `Typewriter` | Caja de diálogo común (páginas, "Siguiente"/"Cerrar", Esc) y cómo se entrega cada línea: con voz (`Voice`) o escribiéndose en silencio (`Typewriter`). Ver [Diálogos](#diálogos-hablar-y-leer). |
| `Readable` | `GameObject` + `Interactable`: algo que se lee (un cartel). Usa la misma caja de diálogo, pero sin voz. |
| `SceneFile` | Parser de `.scene`, sin OpenGL. Lo usa `SceneStage`. (La antigua clase `Scene` se eliminó: la sustituye `SceneStage`.) |
| `Animation` | Rango de frames con nombre. **Todavía no se usa.** |

## Arranque

`main` cambia el directorio de trabajo a `src/`, que localiza junto al ejecutable a partir de `/proc/self/exe` (`test/test` → `test/../src`). Por eso los shaders se abren como `animatedshader.vert` y los assets como `../assets/...`, se lance desde donde se lance. Si falta un shader, `fileToString` lo dice y termina. Antes devolvía una cadena vacía y el driver acababa fallando al enlazar con un error confuso (`must write to gl_Position`).

## Un frame

```
[cambio de mapa pendiente]   si el selector (o el arranque) pidió un mapa: switchMap() aquí, fuera de la UI
camera.resize()              viewport y proyección si cambia el framebuffer
interaction.update(pos)      aviso "E: usar ..."; E abre o cierra el panel del objeto cercano
ui.update()                  ratón y teclas → paneles; Esc cierra el de arriba; sin paneles, Esc → pausa y Z → mapas
controller.setEnabled(!ui.hasPanels())   con cualquier panel abierto, controles en pausa y cursor libre
controller.update()          ratón → rotación de la cámara; teclas → player->control(dir, up, yaw)
stage->update(dt)            update(dt) de cada objeto; a los dinámicos, además, apply() (suelo)
player->followCamera()       la cámara sigue al jugador ya movido
glClear(horizonte del mapa)
stage->render(shader, camPos, t)   cielo del mapa (si tiene) y después cada GameObject
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
├── UIButton                     acción al pulsar y soltar encima (texto fijo o leído cada frame)
├── UISlider                     número en [min, max]; lee y escribe a través de funciones
├── UIInfoRow                    texto a la izquierda y valor a la derecha (p. ej. acción y tecla)
├── UITextBlock                  párrafo con ajuste de línea; puede mostrar solo una fracción (subtítulos)
└── UIContainer (abstracta)      posee a sus hijos
    ├── UIPanel                  ventana: título, botón de cerrar, arrastrable; hijos en vertical
    └── UIRow                    hijos en horizontal, repartiendo el ancho

UIManager    paneles abiertos, reparto del ratón y del teclado, Esc, aviso inferior, dibujo
UIRenderer   rectángulos y texto en píxeles de ventana, en un solo draw call
Interactable contrato entre un objeto del juego y la interfaz
```

- **Coordenadas:** píxeles de ventana con (0, 0) arriba a la izquierda, las mismas que `glfwGetCursorPos`.
- **Ratón:** `UIManager::update` busca el elemento bajo el cursor (`elementAt`, recursivo). Si es interactivo, recibe `onPress`, luego `onDrag` mientras se mantiene el botón y por último `onRelease`. Si no lo es (etiquetas, filas), el clic va a su panel, que se encarga del arrastre por el título y del botón de cerrar. El panel pulsado pasa al frente.
- **Teclado:** `UIManager` instala el *key callback* de GLFW (y el *user pointer* de la ventana), así que **nada más puede instalarlos**. Si algo necesita teclas, que lea con `glfwGetKey` o que pase por `UIManager`. Cada pulsación va al `onKey(key)` del panel de arriba. Si no la gestiona (devuelve `false`) y es Esc, el panel se cierra. Con ningún panel abierto, la tecla ejecuta su **atajo** (`ui.bindKey(tecla, acción)`). En el juego, Esc abre la pausa y Z el selector de mapas.
- **Pausa de controles:** una sola regla en el bucle: `controller.setEnabled(!ui.hasPanels())`. Ningún panel ni sistema tiene que tocar el `Controller`.
- **Valores en vivo:** `UILabel` y `UISlider` no guardan el valor, lo leen cada frame con una `std::function`. Así siempre muestran el estado real del objeto, aunque cambie por otra vía (por ejemplo, mientras el satélite gira).
- **Dibujo:** `UIRenderer` usa su propio shader (`ui.vert`/`ui.frag`) y, al terminar, **vuelve a activar el programa anterior**, porque los setters de `Shader` suponen que el shader del motor está activo. Desactiva el depth test y activa el blending solo mientras dibuja.
- **Texto:** se dibuja con `stb_easy_font.h` (de dominio público y en `src/`), que solo tiene ASCII. `UIRenderer::text` acepta UTF-8 y lo pasa a ASCII con `toAscii`: á → a, ñ → n, ¿ y ¡ desaparecen, ° → " deg", y cualquier otro carácter → "?". Así se puede escribir español correcto (lo que necesita la voz) y en pantalla sale sin tildes.
- **Estilo:** los colores y márgenes son comunes y están en `UITheme` (`UIElement.h`).
- **Abrir paneles:** `ui.open(Interactable&)` coloca el panel a la derecha. `ui.open(new MiPanel(...))` abre cualquier panel (el `UIManager` pasa a ser su dueño) centrado. `ui.close(panel)` y `panel->requestClose()` lo cierran al final del `update`, así que se pueden llamar desde sus propios botones o teclas. `ui.isOpen(panel)` solo compara punteros y es seguro aunque el panel ya no exista.
- **Opciones de `UIPanel`:** el constructor es `UIPanel(título, ancho = 360, closable = true)`. Con `closable = false` no hay botón de cerrar (útil en menús, donde la X confundiría). Si `dimsBackground()` devuelve `true`, el juego se oscurece detrás.

**Hacer que un objeto se pueda usar:**
1. Hereda de `Interactable` e implementa `getInteractionName()`, `getInteractionPoint()` y `buildInterface(UIPanel&)` (opcionalmente también `getInteractionRange()`, que por defecto es 3).
2. En `buildInterface`, añade elementos con `panel.add(new UILabel(...))` y similares. Las lambdas pueden capturar `this`, siempre que el objeto viva más que el panel. Si tiene que reaccionar al abrirse o cerrarse el panel (por ejemplo, un NPC que empieza a hablar y se calla), sobrescribe `onInterfaceOpened(posiciónJugador)` y `onInterfaceClosed()`.
3. Créalo en el mapa (`GameStage`), por ejemplo en `TestStage`: `add(objeto)` e `interactables.push_back(objeto.get())`. Al cargar el mapa, `switchMap` registra todos los de `getInteractables()` en el `InteractionSystem`.

### Crear una ventana nueva (guía)

Para un menú o ventana reutilizable, **hereda de `UIPanel`**, como `PauseMenu` y `OptionsMenu`:

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
- Los menús reciben un **`MenuContext`** (`UIManager`, `Camera`, `Controls`, `quit`) y se lo pasan unos a otros al navegar (Pausa → Opciones → Controles y vuelta). Si un menú nuevo necesita algo más, añádelo a `MenuContext`, no a cada constructor.
- **`OptionsMenu`** ("Opciones") tiene la **sensibilidad** (0.2x–3x, múltiplos de `SENSIVILITY` = 0.005 rad/píxel) y el **FOV** vertical (40–110°, por defecto `DEFAULT_FOV` = 45). Los cambios se aplican al momento sobre la `Camera`. "Restablecer" vuelve a los valores por defecto, y "Volver" o Esc vuelven a la pausa.
- **`ControlsMenu`** ("Controles", desde Opciones) muestra por grupos todas las acciones con su tecla, además de las entradas fijas (ratón, Esc, clic). Por ahora es solo informativo. "Volver" o Esc regresan a Opciones.
- Los menús oscurecen el juego y no tienen botón de cerrar.
- **El mundo no se detiene** con el menú abierto: `stage.update` sigue corriendo (el satélite termina de girar, por ejemplo). Solo se pausan los controles del jugador.
- **Las opciones no se guardan en disco:** duran hasta que se cierra el juego.
- **Para añadir una opción:**
  1. Si el valor no está en una clase, dale un getter y un setter que la apliquen al momento (como `Camera::setFov`).
  2. Añade un `UISlider` (o un `UIButton`) en el constructor de `OptionsMenu`, con sus límites como constantes de la clase.
  3. Inclúyela en "Restablecer".

## Controles y teclas

**`Controls`** (`Controls.h`) es la única fuente de las teclas del juego. Cada acción (`enum class Action`) tiene una tecla (`key(action)`), una descripción (`describe`) y un grupo (`group`). `keyName` da el nombre legible, adaptado a la distribución del teclado. Hay una sola instancia, creada en `main`, que usan:
- `Controller`: las teclas de movimiento;
- `InteractionSystem`: Usar, y también el texto del aviso;
- `PauseMenu`: Salir;
- el atajo del selector de mapas (en `test.cpp`);
- `ControlsMenu`, que lo lista.

| Acción | Tecla por defecto | Dónde se lee |
|---|---|---|
| `MoveForward/Back/Left/Right` | W / S / A / D | `Controller::update` (cada frame) |
| `MoveUp/MoveDown` | Espacio / Mayús izq. | `Controller` (solo sin gravedad) |
| `Use` | E | `InteractionSystem::update` |
| `Quit` | X | `PauseMenu::onKey` y el texto de su botón |
| `Maps` | Z | atajo `ui.bindKey` en `test.cpp` y `MapSelector` (que se cierra con su misma tecla) |

**Fijas (no son `Action`):** Esc es la tecla genérica de "atrás" de la interfaz (`UIManager`): cierra el panel de arriba y abre la pausa. El ratón mira, y el clic izquierdo usa los paneles. Aparecen en `Controls::fixedControls()` para la pantalla de ayuda.

**Regla: no escribas `GLFW_KEY_...` para una función del juego.** Añade una `Action`, con su tecla en el constructor de `Controls`, su texto en `describe` y su grupo en `group`, y léela con `controls.key(Action::...)`. Así aparece sola en la pantalla de controles y se podrá reasignar.

**Para permitir reasignar teclas en el futuro:**
1. Haz una pantalla (por ejemplo a partir de `ControlsMenu`) que, al pulsar una fila, espere la siguiente tecla en `onKey` y llame a `controls.bind(acción, tecla)`.
2. `Controller`, `InteractionSystem` y los menús leen `Controls` cada vez, así que el cambio se aplica al momento.
3. **Excepción: los atajos de `UIManager` se registran por tecla** (`bindKey`). Al reasignar `Maps`, hay que quitar el atajo de la tecla vieja (hoy falta un `unbindKey`) y registrar el de la nueva.
4. Evita los conflictos: comprueba que la tecla no esté ya usada por otra acción ni sea Esc.
5. Guardarlas en disco necesitaría un fichero de configuración, que todavía no existe.

## Mapas y selector de depuración

- Los mapas están en la lista `maps` de `main` (`test.cpp`). Cada uno tiene un **nombre** y una **función que crea su `GameStage`**, que devuelve `nullptr` si falla. Hoy son "Desierto de dia" (`TestStage`) y "Desierto de noche" (`SceneStage` con `desert.scene`).
- **Z** (`Action::Maps`, sin paneles abiertos) abre `MapSelector`, con un botón por mapa; el actual aparece marcado "(actual)". Z o Esc lo cierran.
- **Cambiar de mapa es diferido:** el botón solo apunta el índice (`requestedMap`). El bucle principal llama a `switchMap` al principio del frame siguiente, nunca dentro de un callback de la UI, porque el botón que se pulsó todavía se está ejecutando.
- **`switchMap(i)`** crea el mapa nuevo (si falla, avisa y se queda en el actual). Después:
  1. cierra todos los paneles (`ui.closeAll`), porque pueden apuntar a objetos del mapa viejo;
  2. vacía el `InteractionSystem` y destruye el mapa viejo;
  3. registra los interactuables nuevos;
  4. conecta el `Controller` al nuevo jugador, con la cámara que pide el mapa y mirando al frente;
  5. aplica su entorno (luz, `moonDir`, `fogColor`).
- **Regla:** nada fuera del mapa puede guardar punteros a sus objetos sin limpiarlos en `switchMap`.

**Añadir un mapa:**
- **En datos (lo más fácil):** crea `assets/scenes/mi_mapa.scene` (ver [Ficheros de escena](#ficheros-de-escena-assetsscenesscene)) y añade a `maps` la entrada `{"Mi mapa", [floorMode]() -> std::unique_ptr<GameStage> { return SceneStage::load("../assets/scenes/mi_mapa.scene", "../assets", floorMode); }}`. El visor web también lo mostrará con `--scene`.
- **En código** (si necesita lógica propia, como `TestStage`): hereda de `GameStage` y, en el constructor, rellena `environment`, `cameraDistance`/`cameraHeight` y `player`, llama a `setFloor` y a `add`/`addDynamic` para el contenido, y opcionalmente `setSky` e `interactables`. Si hace falta, sobrescribe `apply()`. Después añádelo a `maps`.

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
  - el botón "Siguiente", que en la última línea pasa a "Cerrar" y cierra el panel;
  - "Esc: terminar".

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

Son mapas descritos como datos. El juego los carga con `SceneStage` (`SceneFile` hace el parseo) y el visor web (`viewer.js`) muestra el mismo fichero, así que los dos parsers deben estar sincronizados. Los mapas hechos en código (`TestStage`) no se pueden ver en el visor.

Va un comando por línea, con los campos separados por espacios. `#` inicia un comentario. Las rutas de modelo son relativas a `assets/`.

| Comando | Argumentos | Notas |
|---|---|---|
| `moon` | `x y z` | Dirección hacia la luna (se normaliza). Debe coincidir con `MOON_DIR` de `assets/sky/generate_sky.py`, porque la luna está pintada en la textura del cielo. |
| `light` | `r g b` | Color de la luz. |
| `fog` | `r g b` | Color del horizonte: niebla y color de fondo. |
| `sky` | `modelo` | Cúpula de cielo (opcional). |
| `floor` | `modelo x y z` | Suelo por el que se anda (`Stage::setFloor`), también dibujado. Es la referencia de `ground`. |
| `player` | `modelo x y z` | Modelo animado del jugador (un `Walker`, con gravedad). `y` puede ser `ground`. Sin `player` hay un `Walker` invisible. |
| `camera` | `distancia altura` | Distancia detrás del jugador y altura sobre su posición, que está en sus pies. **Con distancia 0 es primera persona** y el jugador no se dibuja. En `desert.scene` vale `0 1.6`, la altura de los ojos del pingüino. |
| `object` | `modelo x y z yaw escala [efecto]` | Objeto estático. `yaw` en radianes. `y` es una altura del mundo o **`ground`**: sobre el suelo en (x, z), hundido 0.05 (`SceneStage::PROP_SINK`). Usa `ground` siempre que puedas, porque las dunas se regeneran. Efecto: `lit` (por defecto), `emissive` (pieza con `unlit` = 2) o `breathe` (`setBreathAmp(2)`). |

Si hay un error, el juego muestra `fichero:línea: mensaje` y no cambia de mapa (si es el primero, termina). El visor muestra el mismo mensaje en su barra de estado.

## Pipeline de assets

- `desert/`, `sky/` y `creature/` se generan con `python3 assets/<dir>/generate_*.py` (necesita numpy y Pillow). La salida es reproducible porque usan semilla. **No edites los OBJ ni los JPG a mano: cambia el script y regenera.**
- Las alturas `y` de los objetos del desierto salen de `dune_height(x, z)` en `generate_assets.py`: `y = -1 + dune_height(x, z) - 0.05`. El visor muestra la `y` sugerida al pasar el cursor por las dunas.
- Las texturas se cargan con `stb_image` a partir del `map_Kd`/`map_Ks` del material, relativo al directorio del modelo. Assimp invierte las UV (`aiProcess_FlipUVs`).

## Recetas

**Añadir o mover un objeto.** En un mapa `.scene`, edita el fichero: con el visor abierto se recarga solo, y en el juego se ve al volver a cargar el mapa con Z. En `TestStage` (`test.cpp`), en su constructor: `auto o = make_shared<GameObject>(loadModel("../assets/..."))`, y después `setPosition`/`setYaw`/`setScale` y `add(o)`. Si se mueve solo, crea un `DynamicGameObject` y añádelo con `addDynamic`. Para apoyarlo en el suelo, usa `floorAt(x, z, altura)`.

**Añadir un modelo nuevo.** Copia el OBJ+MTL+textura en `assets/<algo>/` y cárgalo con `loadModel`. El stage lo carga una sola vez, aunque se use en varios objetos.

**Un personaje controlable nuevo.** Hereda de `PlayableCharacter` (ver `Walker` y `RV`): `control()` recibe la entrada, `update(dt)` la convierte en movimiento (con `steerTowards`) y `attachCamera`/`followCamera` deciden la cámara. Después, `controller.attach(personaje, distancia, altura)`.

**Añadir un efecto de sombreado.** Impleméntalo en `animatedshader.vert` y/o `shader.frag`, controlado por un uniform. Fíjalo por objeto en `GameObject::Draw` (como `breathAmp`) o por pieza (como `Part::unlit`). Si el visor web tiene que mostrarlo, copia también el cambio en sus shaders (`viewer.js`).

**Cambiar la cámara del jugador.** `controller.attach(player, distancia, altura)` en `main`. Distancia 0 es primera persona (altura 1.6 = los ojos del pingüino). Con una distancia mayor que 0 es tercera persona y el pingüino vuelve a verse, aunque medio hundido: `AnimatedModel` lo centra en su posición y la posición está en los pies.

## Limitaciones conocidas

- La interfaz muestra el texto en ASCII (`toAscii` quita tildes y eñes). Recibe teclas sueltas (`onKey`), pero no hay campos de texto ni rueda del ratón.
- Las opciones (sensibilidad, FOV) no se guardan entre partidas.
- Solo se reproduce la primera animación del FBX y no hay mezcla entre animaciones (`Animation` está sin usar).
- No hay colisiones entre objetos: solo con el suelo. Con gravedad, Espacio/Shift no hacen nada (no se puede saltar).
- El visor web solo muestra los mapas `.scene` (la noche), no `TestStage`.
- Al cambiar de mapa se vuelven a cargar todos sus modelos (no hay caché entre mapas).
- El volumen no se puede ajustar en Opciones (existe `SoundEngine::setMasterVolume`). La voz de espeak-ng es robótica.
- Los subtítulos avanzan en proporción al tiempo de audio, no palabra a palabra.
- Nunca se liberan los recursos GL (VAO/VBO/texturas). Las texturas no se comparten entre modelos distintos.
- `Model` ignora las transformaciones de los nodos del fichero. Si un OBJ/FBX estático depende de ellas, aparecerá mal colocado.
