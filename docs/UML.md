# Diagramas UML y visión general del proyecto

Este documento resume cómo encajan las clases del proyecto. Los diagramas están en [Mermaid](https://mermaid.js.org/), que **GitHub dibuja directamente** en el navegador (también VS Code con la extensión "Markdown Preview Mermaid Support"). Los detalles de cada módulo están en [ARCHITECTURE.md](ARCHITECTURE.md).

Se leen de lo general a lo concreto:

1. [Vista general](#1-vista-general): subsistemas y cómo se hablan entre sí.
2. [Mundo](#2-mundo-escenarios-y-objetos): escenarios, objetos, personajes y objetos usables.
3. [Física y colisiones](#3-física-y-colisiones): formas de colisión, rejilla del `Stage` y el vehículo.
4. [Renderizado](#4-renderizado): mallas, modelos, animación, cámara y luz.
5. [Entrada e interacción](#5-entrada-e-interacción).
6. [Audio y diálogos](#6-audio-y-diálogos).
7. [Interfaz de usuario y menús](#7-interfaz-de-usuario-y-menús).
8. [Secuencias](#8-secuencias): un frame, la física de un frame y una conversación.
9. [Índice de todas las clases](#9-índice-de-todas-las-clases).

**Imágenes.** Cada diagrama también está exportado como PNG en [`docs/diagrams/`](diagrams/), por si no tienes un visor de Mermaid:

| Diagrama | Imagen |
|---|---|
| 1. Vista general | [`01-vista-general.png`](diagrams/01-vista-general.png) |
| 2. Mundo | [`02-mundo.png`](diagrams/02-mundo.png) |
| 3. Física y colisiones | [`03-fisica-y-colisiones.png`](diagrams/03-fisica-y-colisiones.png) |
| 4. Renderizado | [`04-renderizado.png`](diagrams/04-renderizado.png) |
| 5. Entrada e interacción | [`05-entrada-e-interaccion.png`](diagrams/05-entrada-e-interaccion.png) |
| 6. Audio y diálogos | [`06-audio-y-dialogos.png`](diagrams/06-audio-y-dialogos.png) |
| 7. Interfaz y menús | [`07-interfaz-y-menus.png`](diagrams/07-interfaz-y-menus.png) |
| 8.1 Un frame | [`08a-secuencia-un-frame.png`](diagrams/08a-secuencia-un-frame.png) |
| 8.2 La física de un frame | [`08b-secuencia-fisica.png`](diagrams/08b-secuencia-fisica.png) |
| 8.3 Hablar con un NPC | [`08c-secuencia-hablar-con-npc.png`](diagrams/08c-secuencia-hablar-con-npc.png) |

La imagen de la vista general (la que da una idea de todo el proyecto de un vistazo) es la primera.

**Leyenda de las flechas** (en los diagramas de clases):

| Flecha | Significa |
|---|---|
| `A <\|-- B` | `B` hereda de `A` |
| `A <\|.. B` | `B` implementa la interfaz `A` (herencia múltiple de una clase abstracta pura) |
| `A *-- B` | `A` es dueña de `B` (composición: `B` vive y muere con `A`) |
| `A o-- B` | `A` guarda una referencia o un puntero compartido a `B`, pero no es la única dueña (agregación) |
| `A --> B` | `A` usa `B` de forma estable (puntero o referencia que guarda) |
| `A ..> B` | `A` depende de `B` solo en una llamada o en un parámetro |

Las clases marcadas `<<abstract>>` no se pueden instanciar; `<<interface>>` son clases abstractas puras; `<<enumeration>>` son enumeraciones; `<<struct>>` son datos sin lógica.

## 1. Vista general

`main` (`src/test.cpp`) crea la ventana y los sistemas globales (cámara, controles, interfaz, sonido, luz) y ejecuta el bucle. Todo lo que es contenido del juego vive en un **`GameStage`** (un mapa), que `main` puede cambiar en marcha.

```mermaid
---
title: Subsistemas y flujo de datos
---
flowchart TB
    main["<b>test.cpp (main)</b><br/>ventana, bucle, lista de mapas, TestStage"]

    subgraph entrada ["input/ (entrada)"]
        Controller["Controller<br/>ratón + teclas"]
        Controls["Controls<br/>Action → tecla"]
        InteractionSystem["InteractionSystem<br/>aviso + tecla de uso"]
    end

    subgraph ui ["ui/ (interfaz 2D)"]
        UIManager["UIManager<br/>paneles, ratón, teclas, Esc"]
        Menus["Menús: Pause, Options,<br/>Camera, Controls, MapSelector"]
        UIRenderer["UIRenderer<br/>shader propio + texto"]
    end

    subgraph mundo ["world/ + entities/ (mundo)"]
        GameStage["GameStage (abstracta)<br/>TestStage · SceneStage"]
        Stage["Stage (abstracta)<br/>objetos, suelo, rejilla"]
        Objetos["GameObject → DynamicGameObject<br/>→ PlayableCharacter (RV, Walker)<br/>Npc · Satellite · Readable"]
    end

    subgraph fisica ["physics/"]
        Shapes["CollisionShape<br/>Capsule · Box"]
        Vehicle["VehicleBody<br/>chasis + 4 muelles"]
    end

    subgraph render ["render/"]
        Camera["Camera"]
        Light["Light"]
        Shader["Shader"]
        Models["Model · AnimatedModel<br/>Mesh · Skeleton"]
    end

    subgraph sonido ["audio/ + dialogue/"]
        Dialogue["Dialogue<br/>LineNarrator: Voice · Typewriter"]
        Sound["SoundEngine · Sound<br/>SpeechSynthesizer"]
    end

    Settings["core/Settings<br/>~/.config/3dengine/settings.cfg"]

    main -->|"update(), attach()"| Controller
    main -->|"update(), draw()"| UIManager
    main -->|"update(pos)"| InteractionSystem
    main -->|"update(dt), render()"| GameStage
    main -->|"resize(), follow()"| Camera
    main -->|"setListener()"| Sound

    Controller -->|"control(dir, yaw)"| Objetos
    Controller -->|"gira"| Camera
    Controller -->|"qué tecla"| Controls
    Controls <-->|"claves controls.*"| Settings
    Menus -->|"lee y guarda"| Settings
    Menus -->|"cambia FOV, teclas"| Camera
    Menus -.->|"lee y reasigna"| Controls

    InteractionSystem -->|"open(Interactable)"| UIManager
    InteractionSystem -->|"el más cercano"| Objetos
    UIManager --> UIRenderer
    UIManager --> Menus

    GameStage --> Stage
    Stage -->|"es dueño de"| Objetos
    Objetos -->|"forma de colisión"| Shapes
    Stage -->|"colisiones y suelo"| Shapes
    Objetos -->|"RV"| Vehicle
    Objetos -->|"piezas"| Models
    Objetos -->|"Draw(shader)"| Shader
    Objetos -.->|"habla / lee"| Dialogue
    Dialogue --> Sound

    Camera --> Shader
    Light --> Shader
    UIRenderer --> Shader
    Camera -.->|"sigue a"| Objetos
```

Reglas que sostienen el diseño:

- **`Stage` manda sobre los objetos y `GameStage` sobre el mapa.** Nada fuera del mapa guarda punteros a sus objetos sin limpiarlos en `switchMap`.
- **`Controls` es la única fuente de teclas** y `UIManager` el único dueño del callback de teclado.
- **Un solo shader de mundo** (`shaders/animatedshader.vert` + `shader.frag`); la interfaz usa el suyo (`ui.vert`/`ui.frag`).

## 2. Mundo: escenarios y objetos

Un **`Stage`** es un nivel: es dueño de sus `GameObject` (estáticos) y `DynamicGameObject` (que se mueven), tiene el suelo y la rejilla de colisiones. Es abstracto: cada escenario implementa `apply(objeto, dt)`, la regla que se aplica a cada objeto dinámico justo después de moverse. **`GameStage`** lo convierte en un mapa jugable (entorno, cielo, jugador, cámara, interactuables) y **`TestStage`** / **`SceneStage`** son los dos mapas concretos.

```mermaid
---
title: Mundo - escenarios, objetos y personajes
---
classDiagram
    direction TB

    class Stage {
        <<abstract>>
        -FloorMode floorMode
        -Model floor_mesh
        -float gridCellSize
        +loadModel(path) Model
        +add(GameObject)
        +addDynamic(DynamicGameObject)
        +setFloor(mesh, position) bool
        +floorAt(x, z, height, normal, maxY) bool
        +keepInsideFloor(position, velocity, margin)
        +update(dt)
        +Draw(shader, time)
        #apply(object, dt)*
        #collideWithFloor(object, dt)
        -resolveCollisions()
    }
    class FloorMode {
        <<enumeration>>
        HeightField
        DownwardRay
    }
    class GameStage {
        <<abstract>>
        #Environment environment
        #PlayableCharacter player
        #float cameraDistance
        #float cameraHeight
        #setSky(model)
        #groundAt(x, z, fallback) float
        +render(shader, cameraPosition, time)
        +getPlayer() PlayableCharacter
        +getInteractables() Interactable[]
    }
    class Environment {
        <<struct>>
        +vec3 lightDir
        +vec3 lightColor
        +vec3 horizon
    }
    class TestStage {
        Desierto de día, en código
        -RV rv
    }
    class SceneStage {
        Desierto de noche, desde .scene
        +load(path, assetDir, mode)$ GameStage
    }
    class SceneFile {
        <<struct>>
        +load(path) bool
    }

    class GameObject {
        #Part[] parts
        #AnimatedModel aniModel
        #vec3 position
        #mat4 rotation
        #float scale
        #CollisionShape shape
        #bool collidable
        +addPart(model, unlit) size_t
        +setPartTransform(part, local)
        +setPosition(x, y, z)
        +setYaw(radians)
        +getPose() Pose
        +update(dt)
        +Draw(shader)
    }
    class Part {
        <<struct>>
        +Model model
        +int unlit
        +mat4 local
    }
    class DynamicGameObject {
        #vec3 velocity
        #vec3 acceleration
        #float mass
        #float maxSpeed
        #float gravity
        #bool grounded
        +steerTowards(wanted, responsiveness)
        +update(dt)
        +contactFloor(stage, dt) bool
        +applyCollision(push, velocityChange)
        +getMass() float
    }
    class PlayableCharacter {
        <<abstract>>
        +attachCamera(camera, distance, height)*
        +followCamera()*
        +control(dir, up, cameraYaw)*
    }
    class RV {
        -VehicleBody body
        -float throttle
        -float steering
        +setWheelModels(left, right)
        +contactFloor(stage, dt) bool
        +applyCollision(push, velocityChange)
    }
    class Walker {
        Jugador a pie, 1ª persona si la distancia es 0
    }
    class Npc {
        -string name
        -Voice voice
        -Dialogue dialogue
        +faceTowards(point)
    }
    class Satellite {
        Cabeza orientable en azimut y cénit
        +getMount() GameObject
        +pointAt(azimuth, zenith)
    }
    class Readable {
        Cartel que se lee
        -Typewriter typewriter
        -Dialogue dialogue
    }
    class Interactable {
        <<interface>>
        +getInteractionName() string
        +getInteractionVerb() string
        +getInteractionPoint() vec3
        +getInteractionRange() float
        +buildInterface(panel)*
        +onInterfaceOpened(playerPosition)
        +onInterfaceClosed()
    }
    class CollisionShape {
        <<abstract>>
        forma de colisión del objeto
    }
    class VehicleBody {
        chasis rígido sobre 4 muelles
    }
    class Model
    class AnimatedModel
    class Camera
    class UIPanel

    Stage <|-- GameStage
    GameStage <|-- TestStage
    GameStage <|-- SceneStage
    Stage --> FloorMode
    Stage "1" *-- "*" GameObject : objects
    Stage "1" *-- "*" DynamicGameObject : dynamicObjects
    Stage o-- Model : floor_mesh y caché de modelos
    GameStage *-- Environment
    GameStage o-- PlayableCharacter : player
    GameStage o-- Interactable : interactables
    SceneStage ..> SceneFile : parsea
    TestStage *-- RV

    GameObject <|-- DynamicGameObject
    DynamicGameObject <|-- PlayableCharacter
    DynamicGameObject <|-- Npc
    GameObject <|-- Satellite
    GameObject <|-- Readable
    PlayableCharacter <|-- RV
    PlayableCharacter <|-- Walker
    Interactable <|.. Npc
    Interactable <|.. Satellite
    Interactable <|.. Readable

    GameObject "1" *-- "*" Part
    Part o-- Model
    GameObject o-- AnimatedModel : si es animado
    GameObject *-- CollisionShape : shape
    RV *-- VehicleBody
    PlayableCharacter ..> Camera : attachCamera
    DynamicGameObject ..> Stage : contactFloor
    Interactable ..> UIPanel : buildInterface
    Satellite o-- GameObject : mount (el poste)
```

Cómo se usa:

- **`GameObject`** se dibuja a sí mismo: `Draw(shader)` fija `objposition` y `objrotation` y dibuja cada pieza (`Part`), que puede tener su propia transformación local (las ruedas del RV).
- **`DynamicGameObject`** añade velocidad, masa y gravedad. Quien lo controla (el `Controller` o una IA) **no cambia su posición**: lo guía con aceleración (`steerTowards`) y el `Stage` lo mueve en `update(dt)`.
- **`PlayableCharacter`** es el personaje que maneja el `Controller`. Cada hijo decide cómo responde a la entrada y cómo lo sigue la cámara: `Walker` anda en la dirección de la cámara; `RV` se conduce como un coche (W/S acelera, A/D gira).
- **`Interactable`** es una interfaz: los objetos que el jugador puede usar la implementan junto a su clase de objeto (`Npc`, `Satellite`, `Readable`).

## 3. Física y colisiones

Tres piezas colaboran. **Las formas** (`CollisionShape`) describen el volumen de cada objeto; la **rejilla del `Stage`** decide qué pares hay que comparar; y el **suelo** mantiene los objetos encima del terreno. El RV añade **`VehicleBody`**, su propia simulación de chasis y suspensión.

```mermaid
---
title: Fisica - formas, rejilla del Stage, suelo y vehiculo
---
classDiagram
    direction LR

    class CollisionShape {
        <<abstract>>
        +type() Type
        +bounds(pose, min, max)*
        +floorSamples(pose, points)*
        +collide(a, poseA, b, poseB, contact)$ bool
    }
    class Capsule {
        -float radius
        -float height
        -vec3 base
        +fit(min, max)$ Capsule
        +segment(pose, a, b, radius)
    }
    class Box {
        -vec3 halfExtents
        -vec3 center
        +world(pose, centre, half)
    }
    class Pose {
        <<struct>>
        +vec3 position
        +mat3 rotation
        +float scale
    }
    class Contact {
        <<struct>>
        +vec3 normal
        +float depth
    }

    class Stage {
        <<abstract>>
        -float gridCellSize
        -Body[] bodies
        -map~long, Cell~ gridCells
        -vector~vec3~ triVerts
        -float[] heights
        +setFloor(mesh, position) bool
        +floorAt(x, z, height, normal, maxY) bool
        +keepInsideFloor(position, velocity, margin)
        +getShapeTests() int
        +update(dt)
        #collideWithFloor(object, dt)
        -registerBody(object, dynamic)
        -resolveCollisions()
        -collideBodies(a, b, tested)
        -collideShapeWithFloor(object)
    }
    class Body {
        <<struct>>
        +GameObject object
        +DynamicGameObject dynamic
    }
    class Cell {
        <<struct>>
        +int[] statics
        +int[] dynamics
    }
    class FloorMode {
        <<enumeration>>
        HeightField
        DownwardRay
    }

    class GameObject {
        -CollisionShape shape
        -bool collidable
        +getPose() Pose
    }
    class DynamicGameObject {
        -float mass
        +contactFloor(stage, dt) bool
        +applyCollision(push, velocityChange)
    }
    class RV {
        -VehicleBody body
    }
    class VehicleBody {
        -quat orientation
        -vec3 velocity
        -vec3 angular
        +place(origin, yaw)
        +setInput(throttle, steering)
        +step(dt, floor)
        +getWheels() WheelState[]
    }
    class VehicleParams {
        <<struct>>
        +float mass
        +vec3 centreOfMass
        +float restLength
        +float uprightSoft
        +float uprightHard
        +Wheel[] wheels
        +vec3[] bumpers
    }
    class Wheel {
        <<struct>>
        +vec3 anchor
        +bool steered
    }
    class WheelState {
        <<struct>>
        +float length
        +float steer
        +bool onGround
    }

    CollisionShape <|-- Capsule
    CollisionShape <|-- Box
    CollisionShape ..> Pose
    CollisionShape ..> Contact : collide

    GameObject *-- CollisionShape : shape
    GameObject ..> Pose : getPose
    GameObject <|-- DynamicGameObject
    DynamicGameObject <|-- RV
    RV *-- VehicleBody
    VehicleBody *-- VehicleParams
    note for VehicleParams "es VehicleBody::Params"
    VehicleParams *-- Wheel
    VehicleBody *-- WheelState

    Stage --> FloorMode
    Stage *-- Body : bodies
    Stage *-- Cell : gridCells
    Body o-- GameObject
    Body o-- DynamicGameObject
    Stage ..> CollisionShape : collide, floorSamples
    DynamicGameObject ..> Stage : contactFloor lee floorAt
```

**Un frame de física** (detalle en el [diagrama de secuencia](#82-la-física-de-un-frame)):

1. Cada objeto dinámico avanza (`update(dt)`) y el escenario le aplica `apply()`, que llama a `collideWithFloor`. Si el objeto lleva su propia lógica de suelo (`contactFloor`), como el RV con su suspensión, el `Stage` se la deja.
2. **Forma contra suelo:** los puntos más bajos de la forma (`floorSamples`: esquinas y la cara más baja de una caja, el punto más bajo de una cápsula) no pueden quedar bajo el suelo, **de cualquier lado que esté boca arriba**. Se empuja el objeto hacia arriba con `applyCollision`.
3. **Forma contra forma:** el `Stage` coloca cada objeto dinámico en la rejilla (celdas de 8 m) y **solo compara los pares que comparten celda**. Un choque separa los objetos: se mueve solo el dinámico contra uno estático, y entre dos dinámicos se mueve más el más ligero (`getMass`). Se quita la velocidad con la que se acercaban, sin rebote. Son dos pasadas.
4. Se repite la comprobación del suelo, por si el empujón metió algo en el terreno.

**Suelo:** `FloorMode::HeightField` interpola una rejilla regular de alturas (O(1), no admite voladizos). `FloorMode::DownwardRay` lanza un rayo vertical contra los triángulos, indexados en otra rejilla (admite cualquier malla). Se elige al crear el `Stage` y no cambia.

**`VehicleBody`** (el RV) es un cuerpo rígido con un centro de masas bajo las ruedas. Cada rueda cuelga de un muelle con amortiguador y recorrido limitado; el neumático da tracción, freno y agarre lateral. Sus esquinas del chasis son puntos de contacto elásticos con rozamiento. Un par de autoenderezado ("tentetieso") lo devuelve siempre a apoyarse en las ruedas.

## 4. Renderizado

```mermaid
---
title: Renderizado - mallas, modelos, animacion, camara y luz
---
classDiagram
    direction LR

    class Shader {
        +setInt(name, value)
        +setFloat(name, value)
        +setVector3(name, x, y, z)
        +setMatrix4(name, matrix)
    }
    class Mesh {
        -Vertex[] vertices
        -uint[] indices
        -Texture[] textures
        -bool hasColor
        +Draw(shader)
    }
    class Model {
        +Mesh[] meshes
        +loadModel(path)
        +Draw(shader)
    }
    class AnimatedMesh {
        -AnimatedVertex[] vertices
        +Draw(shader)
    }
    class AnimatedModel {
        +Skeleton skeleton
        +AnimatedMesh[] meshes
        +fitCenter vec3
        +fitScale float
        +Update(seconds)
        +Draw(shader)
    }
    class Skeleton {
        +BoneInfo[] bones
        +mat4 globalInverseTransform
        +mat4[] boneMats
        +Init(root, animation)
        +Update(seconds)
    }
    class BoneInfo {
        <<struct>>
        +string name
        +mat4 offset
    }
    class Vertex {
        <<struct>>
        +vec3 Position
        +vec3 Normal
        +vec2 TexCoords
    }
    class AnimatedVertex {
        <<struct>>
        +ivec4[3] boneIds
        +vec4[3] weights
    }
    class Texture {
        <<struct>>
        +uint id
        +string type
        +string path
    }
    class Animation {
        sin usar todavía
        +string name
        +float start_time
        +float end_time
        +int priority
    }
    class Camera {
        -mat4 projection
        -mat4 view
        -float fov
        -float sensitivity
        +resize()
        +setAngles(yaw, pitch)
        +setFov(degrees)
        +attachTo(target, distance, height)
        +follow()
        +getForward() vec3
    }
    class Light {
        -vec3 color
        -vec3 position
        +moveTo(x, y, z)
        +setColor(r, g, b)
    }
    class GameObject {
        +Draw(shader)
    }

    Model "1" *-- "*" Mesh
    Mesh *-- Vertex
    Mesh *-- Texture
    AnimatedModel "1" *-- "*" AnimatedMesh
    AnimatedModel *-- Skeleton
    AnimatedMesh *-- AnimatedVertex
    AnimatedMesh *-- Texture
    Skeleton *-- BoneInfo
    Mesh ..> Shader : Draw
    AnimatedMesh ..> Shader : Draw
    Model ..> Shader : Draw
    AnimatedModel ..> Shader : Draw
    Camera --> Shader : escribe projection, view, model
    Light --> Shader : escribe lightPosition, lightColor
    Camera --> GameObject : target
    GameObject o-- Model : parts
    GameObject o-- AnimatedModel
```

`Shader` es el punto de unión: la cámara, la luz y cada malla escriben sus uniforms en él. Hay un solo programa para el mundo; el contrato de uniforms está en [ARCHITECTURE.md](ARCHITECTURE.md#contrato-del-shader).

## 5. Entrada e interacción

```mermaid
---
title: Entrada e interaccion
---
classDiagram
    direction LR

    class Controller {
        -float yaw
        -float pitch
        -bool enabled
        +attach(character, distance, height)
        +setEnabled(enabled)
        +update()
    }
    class Controls {
        -int[] keys
        +key(action) int
        +bind(action, key)
        +readFrom(settings)
        +writeTo(settings)
        +keyName(action) string
    }
    class Action {
        <<enumeration>>
        MoveForward
        MoveBack
        MoveLeft
        MoveRight
        Use
        Quit
        Maps
    }
    class Settings {
        +load() bool
        +save() bool
        +has(key) bool
        +getFloat(key, fallback) float
        +setFloat(key, value)
        +getString(key, fallback) string
        +setString(key, value)
    }
    class PlayableCharacter {
        <<abstract>>
        +control(dir, up, cameraYaw)*
        +attachCamera(camera, distance, height)*
    }
    class Camera {
        +setAngles(yaw, pitch)
    }
    class InteractionSystem {
        -Interactable[] targets
        -UIPanel panel
        +add(target)
        +clear()
        +update(playerPosition)
    }
    class Interactable {
        <<interface>>
        +getInteractionName() string
        +getInteractionPoint() vec3
        +getInteractionRange() float
        +buildInterface(panel)*
    }
    class UIManager {
        +open(Interactable) UIPanel
        +setHint(text)
        +hasPanels() bool
    }
    class UIPanel

    Controller --> Camera : gira con el ratón
    Controller --> Controls : qué tecla es cada acción
    Controller --> PlayableCharacter : control(dir, up, yaw)
    Controls --> Action
    Controls ..> Settings : readFrom / writeTo
    InteractionSystem --> Controls : tecla de uso
    InteractionSystem --> Interactable : el más cercano en alcance
    InteractionSystem --> UIManager : abre su panel
    UIManager ..> Interactable : open
    Interactable ..> UIPanel : buildInterface
```

El bucle del `main` hace `controller.setEnabled(!ui.hasPanels())`: con cualquier panel abierto los controles están en pausa y el cursor queda libre. `InteractionSystem` solo abre un panel cuando no hay otro.

## 6. Audio y diálogos

```mermaid
---
title: Audio, voz y dialogos
---
classDiagram
    direction LR

    class Dialogue {
        -string[] lines
        -size_t current
        -LineNarrator narrator
        +buildPanel(panel)
        +start()
        +end()
    }
    class LineNarrator {
        <<interface>>
        +say(text)*
        +stop()*
        +update(dt, position)*
        +isSpeaking() bool
        +progress() float
    }
    class Voice {
        -State state
        -future pending
        +say(text)
        +stop()
        +update(dt, position)
        +progress() float
    }
    class Typewriter {
        Silencioso: letra a letra
        +say(text)
        +progress() float
    }
    class SpeechSynthesizer {
        <<interface>>
        +synthesize(text, settings) AudioClip
    }
    class EspeakSynthesizer {
        Lanza espeak-ng
    }
    class VoiceSettings {
        <<struct>>
        +string language
        +int wordsPerMinute
        +int pitch
    }
    class SoundEngine {
        +isAvailable() bool
        +play(clip, position) Sound
        +setListener(position, forward)
        +setMasterVolume(volume)
    }
    class Sound {
        +isPlaying() bool
        +getCursorSeconds() double
        +getLengthSeconds() double
        +setPosition(position)
        +setVolume(volume)
        +stop()
    }
    class AudioClip {
        <<struct>>
        +float[] samples
        +uint channels
        +uint sampleRate
        +duration() double
    }
    class Npc
    class Readable
    class UIPanel
    class UITextBlock

    LineNarrator <|.. Voice
    LineNarrator <|.. Typewriter
    SpeechSynthesizer <|-- EspeakSynthesizer
    Dialogue --> LineNarrator : entrega cada línea
    Dialogue ..> UIPanel : buildPanel
    Dialogue ..> UITextBlock : subtítulos con progress()
    Voice --> SoundEngine
    Voice --> SpeechSynthesizer : sintetiza en segundo plano
    Voice *-- Sound
    Voice --> VoiceSettings
    SpeechSynthesizer ..> AudioClip : devuelve
    SoundEngine ..> Sound : play() lo devuelve
    Sound o-- AudioClip : clip
    Npc *-- Voice
    Npc *-- Dialogue
    Readable *-- Typewriter
    Readable *-- Dialogue
```

`Dialogue` no sabe si el texto se habla o se lee: delega en un `LineNarrator`. `Voice` (con voz, para los `Npc`) y `Typewriter` (en silencio, para los `Readable`) lo implementan, y `progress()` hace que los subtítulos avancen al ritmo de la entrega. El oyente de `SoundEngine` sigue a la cámara cada frame.

## 7. Interfaz de usuario y menús

```mermaid
---
title: Interfaz de usuario y menus
---
classDiagram
    direction TB

    class UIElement {
        <<abstract>>
        #UIRect rect
        +preferredHeight() float*
        +layout(x, y, width)
        +draw(renderer, state)*
        +elementAt(x, y) UIElement
        +isInteractive() bool
        +onPress(x, y)
        +onDrag(x, y)
        +onRelease(x, y, inside)
        +onKey(key) bool
    }
    class UILabel
    class UIButton
    class UISlider
    class UIInfoRow
    class UITextBlock
    class UIContainer {
        #UIElement[] children
        +add(child) T
        +elementAt(x, y) UIElement
    }
    class UIPanel {
        -string title
        -float width
        -bool closable
        +moveTo(x, y)
        +requestClose()
        +wantsToClose() bool
        +dimsBackground() bool
    }
    class UIRow

    class UIManager {
        -UIPanel[] panels
        -UIRenderer renderer
        -Binding[] bindings
        +open(panel) UIPanel
        +open(Interactable) UIPanel
        +close(panel)
        +closeAll()
        +bindKey(key, action)
        +setHint(text)
        +update()
        +draw()
    }
    class UIRenderer {
        shader propio, restaura el anterior
        +begin(width, height)
        +rect(x, y, w, h, color)
        +frame(x, y, w, h, thickness, color)
        +text(x, y, string, color)
        +end()
        +toAscii(utf8)$ string
    }
    class MenuContext {
        <<struct>>
        +UIManager ui
        +Camera camera
        +Controls controls
        +Settings settings
        +function quit
    }

    class PauseMenu
    class OptionsMenu
    class SettingsMenu {
        <<abstract>>
        Por defecto / Guardar / Volver
        +hasUnsavedChanges() bool*
        +apply() bool*
        +discard()*
        +resetToDefaults()*
        +save()
        +leave()
        +back()
    }
    class CameraMenu {
        Sensibilidad y FOV
    }
    class ControlsMenu {
        Reasigna teclas sobre una copia
    }
    class ConfirmDialog {
        Pregunta con varias respuestas
    }
    class MapSelector {
        Debug, tecla Z
    }
    class Interactable {
        <<interface>>
    }
    class Shader

    UIElement <|-- UILabel
    UIElement <|-- UIButton
    UIElement <|-- UISlider
    UIElement <|-- UIInfoRow
    UIElement <|-- UITextBlock
    UIElement <|-- UIContainer
    UIContainer <|-- UIPanel
    UIContainer <|-- UIRow
    UIContainer "1" *-- "*" UIElement : children

    UIPanel <|-- PauseMenu
    UIPanel <|-- OptionsMenu
    UIPanel <|-- SettingsMenu
    UIPanel <|-- ConfirmDialog
    UIPanel <|-- MapSelector
    SettingsMenu <|-- CameraMenu
    SettingsMenu <|-- ControlsMenu

    UIManager "1" *-- "*" UIPanel : panels, el último arriba
    UIManager *-- UIRenderer
    UIRenderer --> Shader : ui.vert, ui.frag
    UIManager ..> Interactable : open
    PauseMenu ..> MenuContext
    OptionsMenu ..> MenuContext
    SettingsMenu ..> MenuContext
    PauseMenu ..> OptionsMenu : abre
    OptionsMenu ..> CameraMenu : abre
    OptionsMenu ..> ControlsMenu : abre
    SettingsMenu ..> ConfirmDialog : cambios sin guardar
```

`UIManager` es el **único dueño** del callback de teclado de GLFW. Reparte las teclas al panel de arriba (`onKey`); Esc cierra ese panel y, sin paneles, abre la pausa. Todos los menús son `UIPanel` y reciben un `MenuContext` con lo que necesitan (la interfaz, la cámara, los controles, los ajustes y cómo salir).

## 8. Secuencias

### 8.1 Un frame

```mermaid
---
title: Un frame del bucle principal
---
sequenceDiagram
    autonumber
    participant M as main (test.cpp)
    participant I as InteractionSystem
    participant U as UIManager
    participant C as Controller
    participant P as PlayableCharacter
    participant S as GameStage
    participant K as Camera

    M->>M: dt = tiempo desde el frame anterior
    opt hay un cambio de mapa pendiente
        M->>M: switchMap() (diferido, fuera de la UI)
    end
    M->>K: resize()
    M->>I: update(posición del jugador)
    I->>U: setHint() / open(Interactable)
    M->>U: update()
    U-->>M: hasPanels()
    M->>C: setEnabled(!hasPanels)
    M->>C: update()
    C->>K: setAngles(yaw, pitch)
    C->>P: control(dir, up, yaw)
    M->>S: update(dt)
    Note over S: ver 8.2: mover, suelo, colisiones
    M->>P: followCamera()
    P->>K: follow()
    M->>S: render(shader, cámara, tiempo)
    M->>U: draw()
```

### 8.2 La física de un frame

```mermaid
---
title: Stage.update - mover, suelo y colisiones
---
sequenceDiagram
    autonumber
    participant S as Stage (GameStage)
    participant O as GameObject (estáticos)
    participant D as DynamicGameObject
    participant R as RV
    participant V as VehicleBody
    participant X as CollisionShape

    S->>O: update(dt) de cada uno
    loop cada objeto dinámico
        S->>D: update(dt)
        S->>S: apply(objeto, dt) → collideWithFloor
        alt el objeto lleva su propia lógica de suelo (RV)
            S->>R: contactFloor(stage, dt)
            R->>V: step(dt, consulta de suelo)
            V->>S: floorAt(x, z) en cada rueda y esquina
            S-->>V: altura y normal
            V-->>R: posición, orientación, suspensión
            R->>S: keepInsideFloor()
        else el resto
            S->>S: ajusta al suelo (gravedad, snap)
        end
        S->>X: floorSamples(pose)
        S->>S: floorAt() de cada punto
        S->>D: applyCollision(empuje hacia arriba)
    end
    S->>S: resolveCollisions()
    Note over S: coloca los dinámicos en la rejilla y solo compara los pares de la misma celda
    loop pares que comparten celda, 2 pasadas
        S->>X: collide(A, poseA, B, poseB)
        X-->>S: normal y profundidad
        S->>D: applyCollision(empuje, cambio de velocidad)
    end
    loop cada objeto dinámico
        S->>S: forma contra suelo otra vez
    end
```

### 8.3 Hablar con un NPC

```mermaid
---
title: Pulsar E ante un NPC
---
sequenceDiagram
    autonumber
    actor J as Jugador
    participant I as InteractionSystem
    participant U as UIManager
    participant N as Npc
    participant D as Dialogue
    participant V as Voice
    participant E as EspeakSynthesizer
    participant A as SoundEngine

    J->>I: pulsa E (Action::Use)
    I->>I: elige el Interactable más cercano en alcance
    I->>U: open(npc)
    U->>N: buildInterface(panel)
    N->>D: buildPanel(panel)
    U-->>I: panel
    I->>N: onInterfaceOpened(posición del jugador)
    N->>N: faceTowards(jugador)
    N->>D: start()
    D->>V: say(línea)
    V->>E: synthesize(texto) en segundo plano
    E-->>V: AudioClip
    V->>A: reproduce el clip en 3D, en la boca del NPC
    loop mientras habla
        D->>V: progress()
        Note over D: los subtítulos avanzan con el audio
    end
    J->>D: Siguiente / Cerrar / Esc
    D->>V: stop()
    I->>N: onInterfaceClosed()
```


## 9. Índice de todas las clases

Generado a partir de las cabeceras de `src/`. La última columna es la sección donde aparece la clase (un `—` en *Hereda de* significa que no tiene clase base). Los tipos auxiliares que viven dentro de otra clase (`Part`, `VehicleBody::Params`, `Stage::Body`…) se ven en el diagrama de su clase.

| Clase | Fichero | Hereda de | Sección |
|---|---|---|---|
| `Npc` | `src/entities/Npc.h` | `DynamicGameObject`, `Interactable` | 2 |
| `RV` | `src/entities/RV.h` | `PlayableCharacter` | 2 |
| `Readable` | `src/entities/Readable.h` | `GameObject`, `Interactable` | 2 |
| `Satellite` | `src/entities/Satellite.h` | `GameObject`, `Interactable` | 2 |
| `Walker` | `src/entities/Walker.h` | `PlayableCharacter` | 2 |
| `TestStage` | `src/test.cpp` | `GameStage` | 2 |
| `DynamicGameObject` | `src/world/DynamicGameObject.h` | `GameObject` | 2 |
| `GameObject` | `src/world/GameObject.h` | — | 2 |
| `Environment` | `src/world/GameStage.h` | — | 2 |
| `GameStage` | `src/world/GameStage.h` | `Stage` | 2 |
| `Interactable` | `src/world/Interactable.h` | — | 2 |
| `PlayableCharacter` | `src/world/PlayableCharacter.h` | `DynamicGameObject` | 2 |
| `Effect` | `src/world/SceneFile.h` | — | 2 |
| `SceneFile` | `src/world/SceneFile.h` | — | 2 |
| `SceneObject` | `src/world/SceneFile.h` | — | 2 |
| `SceneStage` | `src/world/SceneStage.h` | `GameStage` | 2 |
| `FloorMode` | `src/world/Stage.h` | — | 2 |
| `Stage` | `src/world/Stage.h` | — | 2 |
| `Box` | `src/physics/CollisionShape.h` | `CollisionShape` | 3 |
| `Capsule` | `src/physics/CollisionShape.h` | `CollisionShape` | 3 |
| `CollisionShape` | `src/physics/CollisionShape.h` | — | 3 |
| `Contact` | `src/physics/CollisionShape.h` | — | 3 |
| `Pose` | `src/physics/CollisionShape.h` | — | 3 |
| `VehicleBody` | `src/physics/VehicleBody.h` | — | 3 |
| `AnimatedMesh` | `src/render/AnimatedMesh.h` | — | 4 |
| `AnimatedVertex` | `src/render/AnimatedMesh.h` | — | 4 |
| `AnimatedModel` | `src/render/AnimatedModel.h` | — | 4 |
| `Animation` | `src/render/Animation.h` | — | 4 |
| `Camera` | `src/render/Camera.h` | — | 4 |
| `Light` | `src/render/Light.h` | — | 4 |
| `Mesh` | `src/render/Mesh.h` | — | 4 |
| `Texture` | `src/render/Mesh.h` | — | 4 |
| `Vertex` | `src/render/Mesh.h` | — | 4 |
| `Model` | `src/render/Model.h` | — | 4 |
| `Shader` | `src/render/Shader.h` | — | 4 |
| `BoneInfo` | `src/render/Skeleton.h` | — | 4 |
| `Skeleton` | `src/render/Skeleton.h` | — | 4 |
| `Settings` | `src/core/Settings.h` | — | 5 |
| `Controller` | `src/input/Controller.h` | — | 5 |
| `Action` | `src/input/Controls.h` | — | 5 |
| `Controls` | `src/input/Controls.h` | — | 5 |
| `InteractionSystem` | `src/input/InteractionSystem.h` | — | 5 |
| `AudioClip` | `src/audio/AudioClip.h` | — | 6 |
| `EspeakSynthesizer` | `src/audio/EspeakSynthesizer.h` | `SpeechSynthesizer` | 6 |
| `Sound` | `src/audio/SoundEngine.h` | — | 6 |
| `SoundEngine` | `src/audio/SoundEngine.h` | — | 6 |
| `SpeechSynthesizer` | `src/audio/SpeechSynthesizer.h` | — | 6 |
| `VoiceSettings` | `src/audio/SpeechSynthesizer.h` | — | 6 |
| `Voice` | `src/audio/Voice.h` | `LineNarrator` | 6 |
| `Dialogue` | `src/dialogue/Dialogue.h` | — | 6 |
| `LineNarrator` | `src/dialogue/LineNarrator.h` | — | 6 |
| `Typewriter` | `src/dialogue/Typewriter.h` | `LineNarrator` | 6 |
| `CameraMenu` | `src/ui/CameraMenu.h` | `SettingsMenu` | 7 |
| `ConfirmDialog` | `src/ui/ConfirmDialog.h` | `UIPanel` | 7 |
| `ControlsMenu` | `src/ui/ControlsMenu.h` | `SettingsMenu` | 7 |
| `MapSelector` | `src/ui/MapSelector.h` | `UIPanel` | 7 |
| `MenuContext` | `src/ui/MenuContext.h` | — | 7 |
| `OptionsMenu` | `src/ui/OptionsMenu.h` | `UIPanel` | 7 |
| `PauseMenu` | `src/ui/PauseMenu.h` | `UIPanel` | 7 |
| `SettingsMenu` | `src/ui/SettingsMenu.h` | `UIPanel` | 7 |
| `UIButton` | `src/ui/UIButton.h` | `UIElement` | 7 |
| `UIContainer` | `src/ui/UIContainer.h` | `UIElement` | 7 |
| `UIElement` | `src/ui/UIElement.h` | — | 7 |
| `UIInfoRow` | `src/ui/UIInfoRow.h` | `UIElement` | 7 |
| `UILabel` | `src/ui/UILabel.h` | `UIElement` | 7 |
| `UIManager` | `src/ui/UIManager.h` | — | 7 |
| `UIPanel` | `src/ui/UIPanel.h` | `UIContainer` | 7 |
| `UIRenderer` | `src/ui/UIRenderer.h` | — | 7 |
| `UIRow` | `src/ui/UIRow.h` | `UIContainer` | 7 |
| `UISlider` | `src/ui/UISlider.h` | `UIElement` | 7 |
| `UITextBlock` | `src/ui/UITextBlock.h` | `UIElement` | 7 |

## Cómo mantener este documento

- Si añades una clase, ponla en el diagrama de su subsistema (con sus relaciones, no hace falta listar todos los miembros) y en el índice.
- Si cambias una herencia, un dueño (`*--`) o una dependencia importante, actualiza la flecha. Es lo que más se desfasa.
- **Los PNG de `docs/diagrams/` hay que regenerarlos** cuando cambie un diagrama: son una copia del bloque Mermaid. Se hace con mermaid-cli (`mmdc -p cfg.json -i bloque.mmd -o imagen.png -s 2`; ver `CLAUDE.md`).
- Los diagramas son texto: se editan a mano y se ven en GitHub. Para comprobar que la sintaxis es válida antes de subir el cambio, se puede pasar cada bloque por el parser de Mermaid (`mermaid.parse`).
