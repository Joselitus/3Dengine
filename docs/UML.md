# UML diagrams and project overview

This document summarises how the project's classes fit together. The diagrams are written in [Mermaid](https://mermaid.js.org/), which **GitHub renders directly** in the browser (VS Code too, with the "Markdown Preview Mermaid Support" extension). The details of each module are in [ARCHITECTURE.md](ARCHITECTURE.md).

They read from the general to the specific:

1. [Overview](#1-overview): subsystems and how they talk to each other.
2. [World](#2-world-stages-and-objects): stages, objects, characters and usable objects.
3. [Creatures and encounters](#3-creatures-and-encounters): the night creatures, Bob and his saucer, safe spaces.
4. [Physics and collisions](#4-physics-and-collisions): collision shapes, the `Stage` grid and the vehicle.
5. [Rendering](#5-rendering): meshes, models, animation, camera, lights and effects.
6. [Input and interaction](#6-input-and-interaction).
7. [Audio and dialogue](#7-audio-and-dialogue).
8. [User interface and menus](#8-user-interface-and-menus).
9. [Sequences](#9-sequences): one frame, one frame of physics, a conversation, and getting in and out of the RV.
10. [Index of all classes](#10-index-of-all-classes).

**Images.** The PNG exports in [`docs/diagrams/`](diagrams/) are copies of the *previous* (Spanish) version of the diagrams and have **not** been regenerated for this revision. The Mermaid blocks in this file are the source of truth.

**Arrow legend** (class diagrams):

| Arrow | Meaning |
|---|---|
| `A <\|-- B` | `B` inherits from `A` |
| `A <\|.. B` | `B` implements the interface `A` (multiple inheritance from a pure abstract class) |
| `A *-- B` | `A` owns `B` (composition: `B` lives and dies with `A`) |
| `A o-- B` | `A` keeps a reference or shared pointer to `B` but is not its only owner (aggregation) |
| `A --> B` | `A` uses `B` in a lasting way (a pointer or reference it stores) |
| `A ..> B` | `A` depends on `B` only through a call or a parameter |

Classes marked `<<abstract>>` cannot be instantiated; `<<interface>>` are pure abstract classes; `<<enumeration>>` are enums; `<<struct>>` are plain data.

## 1. Overview

`main` (`src/test.cpp`) creates the window and the global systems (camera, controls, UI, sound, light) and runs the loop. All game content lives in a **`GameStage`** (a map) that `main` can swap while running. Maps are listed in `main()` (name + factory) and chosen with the debug selector (Z) or `--map`.

Most maps are a **`VehicleStage`**: it adds the RV, the penguin on foot, the day/night cycle and the night creatures. The five maps are: the daytime desert (`TestStage`), the road forest (`ForestStage`), the pine forest (`PineForestStage`), Route 66 (`Route66Stage`) and the night desert (`SceneStage`, loaded from a `.scene` file).

```mermaid
---
title: Subsystems and data flow
---
flowchart TB
    main["<b>test.cpp (main)</b><br/>window, loop, map list, TestStage"]

    subgraph input ["input/"]
        Controller["Controller<br/>mouse + keys"]
        Controls["Controls<br/>Action → key"]
        InteractionSystem["InteractionSystem<br/>hint + use key"]
    end

    subgraph ui ["ui/ (2D interface)"]
        UIManager["UIManager<br/>panels, mouse, keys, Esc"]
        Menus["Menus: Pause, Options, Camera,<br/>Controls, Audio, MapSelector,<br/>CommandConsole"]
        Overlays["Overlays: Death, Credits,<br/>Struggle, Crosshair"]
        UIRenderer["UIRenderer<br/>own shader + text"]
    end

    subgraph world ["world/ + entities/"]
        GameStage["GameStage (abstract)<br/>VehicleStage: TestStage · ForestStage ·<br/>PineForestStage · Route66Stage<br/>SceneStage"]
        Stage["Stage (abstract)<br/>objects, floor, grid, clock"]
        Objects["GameObject → DynamicGameObject<br/>→ PlayableCharacter: RV · Walker · Saucer<br/>Npc: Pingu · FollaCulos · Mosquito · Bob · Flatwoods<br/>Satellite · Readable · FuelPump"]
        Safe["SafeSpace<br/>boxes where creatures cannot attack"]
    end

    subgraph physics ["physics/"]
        Shapes["CollisionShape<br/>Capsule · Box · CompoundShape"]
        Vehicle["VehicleBody · EngineSimulator<br/>ImpactDetector"]
        Procedural["ProceduralPose: Ragdoll ·<br/>SpiderGait · InsectLegs"]
    end

    subgraph render ["render/ + effects/"]
        Camera["Camera"]
        Light["Light · SpotLight"]
        Shader["Shader"]
        Models["Model · AnimatedModel<br/>Mesh · Skeleton"]
        Effects["ParticleEmitter · ParticleRenderer<br/>Explosion · FilmGrain"]
    end

    subgraph sound ["audio/ + dialogue/"]
        Dialogue["Dialogue<br/>LineNarrator: Voice · Typewriter"]
        Sound["SoundEngine · Sound · MusicPlayer<br/>SpeechSynthesizer · EngineSynth · BuzzSynth"]
    end

    Settings["core/Settings<br/>~/.config/3dengine/settings.cfg"]
    Commands["core/Commands<br/>/reset /day /night"]

    subgraph debug ["debug/"]
        DebugSelector["DebugSelector<br/>keys 1, 2, 0: select, place, edit"]
    end

    main -->|"update(), attach()"| Controller
    main -->|"update(), draw()"| UIManager
    main -->|"update(pos)"| InteractionSystem
    main -->|"update(dt), render()"| GameStage
    main -->|"resize(), follow()"| Camera
    main -->|"setListener()"| Sound
    main -->|"update(), draw()"| DebugSelector
    main -->|"mirror passes, FilmGrain"| Effects

    Controller -->|"control(dir, up, yaw)"| Objects
    Controller -->|"turns"| Camera
    Controller -->|"which key"| Controls
    Controls <-->|"controls.* keys"| Settings
    Menus -->|"read and save"| Settings
    Menus -->|"FOV, keys, volume"| Camera
    Menus -.->|"read and rebind"| Controls
    Menus --> Commands
    Commands -->|"clock, reset"| GameStage

    InteractionSystem -->|"open(Interactable)"| UIManager
    InteractionSystem -->|"the closest one"| Objects
    UIManager --> UIRenderer
    UIManager --> Menus
    UIManager --> Overlays
    DebugSelector -->|"overlay: cross + data"| UIManager
    DebugSelector -.->|"ray, describe()"| Objects
    DebugSelector -->|"shape and AABB (LineRenderer)"| Camera

    GameStage --> Stage
    GameStage --> Safe
    Stage -->|"owns"| Objects
    Objects -->|"collision shape"| Shapes
    Stage -->|"collisions and floor"| Shapes
    Objects -->|"RV"| Vehicle
    Objects -->|"creature poses"| Procedural
    Objects -->|"parts"| Models
    Objects -->|"Draw(shader)"| Shader
    Stage -->|"emitters"| Effects
    Objects -.->|"speak / read"| Dialogue
    Objects -.->|"sounds"| Sound
    Dialogue --> Sound

    Camera --> Shader
    Light --> Shader
    UIRenderer --> Shader
    Effects --> Shader
    Camera -.->|"follows"| Objects
```

Rules that hold the design together:

- **`Stage` rules the objects and `GameStage` rules the map.** Nothing outside the map keeps pointers to its objects without clearing them in `switchMap` (always deferred through `requestedMap`, never called from a UI callback).
- **`Controls` is the only source of keys** and `UIManager` the only owner of the keyboard callback. Every game function is an `Action`.
- **One world shader** (`shaders/animatedshader.vert` + `shader.frag`); the UI uses its own (`ui.vert`/`ui.frag`), and there are separate ones for particles, debug lines and film grain.
- **`main` is the other person's code**: our classes inherit from theirs, not the other way round.

## 2. World: stages and objects

A **`Stage`** is a level: it owns its `GameObject`s (static) and `DynamicGameObject`s (moving), holds the floor, the collision grid and the day clock. It is abstract: each stage implements `apply(object, dt)`, the rule applied to every dynamic object right after it moves. **`GameStage`** turns it into a playable map (environment, sky, player, camera, interactables) and **`VehicleStage`** adds the RV, the player on foot, the light cycle and the creatures. The concrete maps inherit from it.

```mermaid
---
title: World - stages, objects and characters
---
classDiagram
    direction TB

    class Stage {
        <<abstract>>
        -FloorMode floorMode
        -Model floor_mesh
        -float gridCellSize
        -AudioClip music
        -float timeOfDay
        -float dayDuration
        -float timeScale
        -SafeSpace[] safeSpaces
        +add(GameObject)
        +addDynamic(DynamicGameObject)
        +addDynamicLater(object)
        +removeLater(object)
        +relocate(object, position)
        +turn(object, radians)
        +setFloor(mesh, position, materials) bool
        +floorAt(x, z, height, normal, maxY) bool
        +materialAt(x, z) FloorMaterial
        +keepAboveFloor(position)
        +addSafeSpace(space)
        +isSheltered(point) bool
        +get/setTimeOfDay(hours)
        +setTimeScale(factor)
        +getProperties(properties)
        +update(dt)
        +Draw(shader, time)
        #apply(object, dt)*
        #onTimeChanged()
    }
    class FloorMode {
        <<enumeration>>
        HeightField
        DownwardRay
    }
    class FloorMaterial {
        <<enumeration>>
        Sand
        Asphalt
        Grass
    }
    class MaterialMap {
        +uniform(material)$ MaterialMap
        +loadImage(path)$ MaterialMap
        +at(u, v) FloorMaterial
    }
    class SafeSpace {
        A box in an object's frame or fixed in the world
        +contains(point) bool
    }
    class GameStage {
        <<abstract>>
        #Environment environment
        #PlayableCharacter player
        +render(shader, cameraPosition, time)
        +getPlayer() PlayableCharacter
        +getInteractables() Interactable[]
        +setPlayer(player, distance, height, yaw)
        +takePlayerChange() bool
        +leaveVehicle()
        +interactionsEnabled() bool
        +farPlane() float
        +playerImmobilized() bool
        +abductPlayer()
        +isPlayerDead() bool
        +alienPresence() float
        +struggleProgress() float
        +rearMirror(side) MirrorView
        +setMirrorView(bool)
        +fire()
    }
    class Environment {
        <<struct>>
        +vec3 lightDir
        +vec3 lightColor
        +vec3 horizon
        +vec3 skyZenith
        +float starAlpha
        +canopyMask
        +forestHorizon
    }
    class VehicleStage {
        <<abstract>>
        RV, penguin on foot, light cycle, controls
        #createRV()
        #createWalker()
        #createCreature()
        #createAlienVisit()
        #createFlatwoods()
        +playerSheltered() bool
        +playerInRV() bool
        -enterRV()
        -enterSaucer()
        -updatePossession(dt)
        #onTimeChanged()
    }
    class TestStage {
        Daytime desert, in code
    }
    class ForestStage {
        3 km road, about 14600 trees in 3 LODs
    }
    class PineForestStage {
        200 x 200 m, Poisson-disk pines, canopy shadows
    }
    class Route66Stage {
        20 km road, gas station at km 10
    }
    class SceneStage {
        Night desert, from a .scene file
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
        +addDetail() / selectDetail()
        +setPartVisible(part, visible)
        +setPosition(x, y, z)
        +getPose() Pose
        +update(dt)
        +takeDamage(amount)
        +describe(lines)
        +getProperties(properties)
        +teleport(position)
        +turn(radians)
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
        #float mass
        #float drag
        #float maxSpeed
        #bool grounded
        +addMesh(model) size_t
        +setMesh(index)
        +steerTowards(wanted, responsiveness)
        +getStepHeight() float
        +contactFloor(stage, dt) bool
        +applyCollision(push, velocityChange)
    }
    class PlayableCharacter {
        <<abstract>>
        +attachCamera(camera, distance, height)*
        +followCamera()*
        +control(dir, up, cameraYaw)*
        +setRunning(bool)
    }
    class RV {
        Hollow body, walkable inside
        -VehicleBody body
        -EngineSimulator engine
        -ImpactDetector impact
        -bool onFire
        +interiorBox() Box
        +steeringInteraction() Interactable
        +toggleDoor()
        +wreck()
        +explodeEngine()
        +setMirrorTexture(side, texture)
        +setFuelPerMeter(f)
    }
    class Walker {
        Penguin on foot, first person, flashlight
    }
    class Npc {
        -string name
        -Voice voice
        -Dialogue dialogue
        +faceTowards(point)
        +startRagdoll() / endRagdoll()
        +onInteraction(interaction)
    }
    class Pingu {
        Dances; while talking, stands and breathes
    }
    class Satellite {
        Dish aimable in azimuth and zenith
        +getMount() GameObject
        +pointAt(azimuth, zenith)
    }
    class Readable {
        Sign that is read
        -Typewriter typewriter
    }
    class FuelPump {
        Gas pump: fills the RV tank
    }
    class Interactable {
        <<interface>>
        +getInteractionName() string
        +getInteractionVerb() string
        +getInteractionPoint() vec3
        +getInteractionRange() float
        +isInteractionAvailable() bool
        +usesDirectly() bool
        +onUse(playerPosition)
        +buildInterface(panel)*
        +onInterfaceOpened(playerPosition)
        +onInterfaceClosed()
    }
    class CollisionShape {
        <<abstract>>
    }
    class Model
    class AnimatedModel
    class Camera
    class UIPanel
    class ParticleEmitter

    Stage <|-- GameStage
    GameStage <|-- VehicleStage
    GameStage <|-- SceneStage
    VehicleStage <|-- TestStage
    VehicleStage <|-- ForestStage
    VehicleStage <|-- PineForestStage
    VehicleStage <|-- Route66Stage
    Stage --> FloorMode
    Stage o-- MaterialMap : floorMaterials (required with HeightField)
    MaterialMap ..> FloorMaterial
    Stage "1" *-- "*" GameObject : objects
    Stage "1" *-- "*" DynamicGameObject : dynamicObjects
    Stage "1" *-- "*" SafeSpace : safeSpaces
    Stage o-- Model : floor_mesh and model cache
    Stage "1" *-- "*" ParticleEmitter : emitters
    GameStage *-- Environment
    GameStage o-- PlayableCharacter : player
    GameStage o-- Interactable : interactables
    SceneStage ..> SceneFile : parses
    VehicleStage o-- RV
    VehicleStage o-- Walker : the player on foot

    GameObject <|-- DynamicGameObject
    DynamicGameObject <|-- PlayableCharacter
    DynamicGameObject <|-- Npc
    Npc <|-- Pingu
    GameObject <|-- Satellite
    GameObject <|-- Readable
    GameObject <|-- FuelPump
    PlayableCharacter <|-- RV
    PlayableCharacter <|-- Walker
    Interactable <|.. Npc
    Interactable <|.. Satellite
    Interactable <|.. Readable
    Interactable <|.. FuelPump
    Interactable <|.. RV

    GameObject "1" *-- "*" Part
    Part o-- Model
    GameObject o-- AnimatedModel : if animated
    GameObject *-- CollisionShape : shape
    PlayableCharacter ..> Camera : attachCamera
    DynamicGameObject ..> Stage : contactFloor
    Interactable ..> UIPanel : buildInterface
    Satellite o-- GameObject : mount (the post)
    RV "1" *-- "4" ParticleEmitter : dust, one per wheel
```

How it is used:

- **`GameObject`** draws itself: `Draw(shader)` sets `objposition` and `objrotation` and draws every `Part`, each with its own local transform (the RV's wheels, doors and needles). `addDetail`/`selectDetail` give a tree several levels of detail. `takeDamage` does nothing by default; creatures override it so the saucer's gun can hurt them.
- **`DynamicGameObject`** adds velocity, mass and gravity. Whoever drives it (the `Controller` or an AI) **does not change its position**: it steers with acceleration (`steerTowards`) and the `Stage` moves it in `update(dt)`. To move an object of a stage use `Stage::relocate` / `turn`, never `setPosition`.
- **`PlayableCharacter`** is what the `Controller` drives. `Walker` walks relative to the camera; `RV` drives like a car; `Saucer` flies (see [section 3](#3-creatures-and-encounters)).
- **`Interactable`** is an interface implemented next to the object class. By default using one opens a panel; the `RV` is used directly (`usesDirectly`): the `RV` itself is the **door** and `RV::steeringInteraction()` the **steering wheel** (see [9.4](#94-getting-in-and-out-of-the-rv)).
- **Map change of player.** `setPlayer`/`takePlayerChange` let a stage hand the `Controller` a different character (on foot ↔ RV ↔ saucer).

## 3. Creatures and encounters

The night creatures are created by `VehicleStage`. They never attack a player who is *sheltered* (`playerSheltered()`: driving the RV, piloting the saucer or standing inside a `SafeSpace`), and behave as if the player were in a vehicle.

```mermaid
---
title: Creatures, Bob and the saucer
---
classDiagram
    direction LR

    class DynamicGameObject
    class PlayableCharacter {
        <<abstract>>
    }
    class Interactable {
        <<interface>>
    }
    class Npc
    class FollaCulos {
        Night creature
        States: Pursuit, Caution, RunAway, Hunt
        -SpiderGait gait
        -bool dead
        -bool criticalCondition
        +kill()
        +takeDamage(amount)
        +getLight(lights)
    }
    class Mosquito {
        Giant flying mosquito
        States: Wander, Stalk, Dive, Retreat, ToWater, Lay, Siphon, TireAttack
        -InsectLegs legs
        -float blood
        -float stomach
        +takeDamage(amount)
    }
    class MosquitoEgg {
        Hatches into a small Mosquito
    }
    class Flatwoods {
        Only seen in the mirrors
        Possesses the player in the RV
    }
    class Bob {
        Grey alien
        States: ExitingShip, Wandering, Chasing, Firing, Grabbing, Fallen, Returning
        -float presence
        +getPresence() float
        +struggleOnce()
        +takeDamage(amount)
    }
    class Saucer {
        Phases: Away, Arriving, Landing, RampDown, Landed, RampUp, TakingOff, Leaving
        -SpotLight beam
        -bool gunOut
        +fire()
        +canDisembark() bool
        +getEmitters()
    }
    class AlienVisit {
        <<struct>>
        +Saucer saucer
        +Bob bob
        +create(stage, landing, rampYaw, callbacks)$ AlienVisit
    }
    class SafeSpace
    class VehicleStage {
        <<abstract>>
        +playerSheltered() bool
    }
    class RV {
        +interiorBox() Box
    }
    class Explosion {
        Shared by Mosquito and RV
        +emitters, blast sound
    }
    class ProceduralPose {
        <<abstract>>
        +step(dt)*
        +boneGlobals()*
        +isFinished() bool
    }
    class Ragdoll
    class SpiderGait
    class InsectLegs
    class NpcRagdoll
    class FilmGrain {
        Screen grain, opacity = presence
    }
    class GameStage {
        <<abstract>>
    }

    DynamicGameObject <|-- PlayableCharacter
    DynamicGameObject <|-- Npc
    DynamicGameObject <|-- Mosquito
    DynamicGameObject <|-- MosquitoEgg
    DynamicGameObject <|-- Flatwoods
    DynamicGameObject <|-- Bob
    Npc <|-- FollaCulos
    PlayableCharacter <|-- Saucer
    PlayableCharacter <|-- RV
    Interactable <|.. Saucer
    Interactable <|.. Npc

    ProceduralPose <|-- Ragdoll
    ProceduralPose <|-- SpiderGait
    ProceduralPose <|-- InsectLegs
    FollaCulos *-- SpiderGait
    Mosquito *-- InsectLegs
    Npc ..> NpcRagdoll : startRagdoll
    NpcRagdoll *-- Ragdoll
    Mosquito ..> MosquitoEgg : lays
    Mosquito ..> Explosion
    RV ..> Explosion

    AlienVisit o-- Saucer
    AlienVisit o-- Bob
    Bob --> Saucer : returns to it
    Saucer ..> Bob : lowers ramp when near
    VehicleStage ..> AlienVisit : createAlienVisit
    VehicleStage ..> FollaCulos : createCreature
    VehicleStage ..> Mosquito
    VehicleStage ..> Flatwoods : createFlatwoods
    VehicleStage ..> SafeSpace : playerSheltered
    RV ..> SafeSpace : interior
    GameStage ..> FilmGrain : alienPresence
    Bob ..> GameStage : abduct, paralyse (callbacks)
```

- **`FollaCulos`** runs at the player at night (4.34 m/s) and flees by day; with the player in the RV it circles at 22 m without entering. It hunts other `Npc`s (devours them as a ragdoll) and dies in a hard crash, sticking to the windscreen when hit head-on.
- **`Mosquito`** attacks at dawn and dusk and otherwise looks for water to lay eggs (`MosquitoEgg`). It siphons fuel, can puncture a tyre and bites to the death. It is killed by an external hit above 5 m/s or by two ray-gun shots.
- **`Bob` and the `Saucer`** come only at night (`AlienVisit::create` builds both for any map). Bob wanders by the ship, chases a player on foot, can paralyse them with a yellow eye beam, grab them (button-mash Left Shift to break free) or abduct them. With the ramp down the player can board and **pilot the saucer** (it is a `PlayableCharacter`), and take out a ray gun with F (left click fires: `GameStage::fire` → `Saucer::fire` → `GameObject::takeDamage`).
- **`Flatwoods`** only appears in the rear-view mirrors (`GameStage::setMirrorView`) and only if the player is *in* the RV (`SafeSpace` does not protect). If it catches up it possesses the player: lets go of the RV, opens the door and puts the player out; Left Shift frees them.
- **`ProceduralPose`** is the base for poses generated by code (no animation file): `Ragdoll` (Verlet), `SpiderGait` (four-legged gait with IK) and `InsectLegs`.
- **Presence.** `Bob::getPresence` (higher when he looks at you and approaches, maximal during the abduction) drives the hiss volume and the `FilmGrain` opacity.

## 4. Physics and collisions

Three pieces cooperate. **The shapes** (`CollisionShape`) describe each object's volume; the **`Stage` grid** decides which pairs to compare; and the **floor** keeps objects above the terrain. The RV adds **`VehicleBody`**, its own chassis and suspension simulation.

```mermaid
---
title: Physics - shapes, Stage grid, floor and vehicle
---
classDiagram
    direction LR

    class CollisionShape {
        <<abstract>>
        +type() Type
        +bounds(pose, min, max)*
        +floorSamples(pose, points)*
        +raycast(pose, origin, direction, distance)* bool
        +collide(a, poseA, b, poseB, contact)$ bool
    }
    class Capsule {
        -float radius
        -float height
        +fit(min, max)$ Capsule
    }
    class Box {
        -vec3 halfExtents
        -vec3 center
    }
    class CompoundShape {
        Several boxes with their own position and rotation
        +add(box, position, rotation)
        +setEnabled(part, bool)
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
        +bool top
    }

    class Stage {
        <<abstract>>
        -float gridCellSize
        -Body[] bodies
        -map~long, Cell~ gridCells
        +setFloor(mesh, position, materials) bool
        +floorAt(x, z, height, normal, maxY) bool
        +keepAboveFloor(position)
        +update(dt)
        #collideWithFloor(object, dt)
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
        +getStepHeight() float
        +contactFloor(stage, dt) bool
        +applyCollision(push, velocityChange)
    }
    class RV {
        -VehicleBody body
        -EngineSimulator engine
        -ImpactDetector impact
        -bool damagedWindshield
        -bool onFire
        +hullShape() CompoundShape
        +wreck()
        +explodeEngine()
    }
    class ImpactDetector {
        Violent frontal crash: an abrupt stop right after the hit
        +onCollision(speedBefore, speedAfter, heading, away)
        +update(dt, forwardSpeed)
        +violent() bool
        +severity() float
    }
    class EngineSimulator {
        Key delay, cranking, sputter, catching, running
        +startAttempt(fuel, failChance)
        +hasGivenUp() bool
    }
    class VehicleBody {
        -quat orientation
        -vec3 velocity
        -vec3 angular
        +place(origin, yaw)
        +setInput(throttle, steering)
        +step(dt, floor)
        +setSurfaceQuery(query)
        +setFlat(wheel, bool)
        +setWrecked(bool)
        +getWheels() WheelState[]
    }
    class Surface {
        <<struct>>
        +float grip
        +float rolling
        +float topSpeed
    }
    class VehicleParams {
        <<struct>>
        +float mass
        +vec3 centreOfMass
        +float restLength
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
    CollisionShape <|-- CompoundShape
    CompoundShape "1" *-- "*" Box
    CollisionShape ..> Pose
    CollisionShape ..> Contact : collide

    GameObject *-- CollisionShape : shape
    GameObject ..> Pose : getPose
    GameObject <|-- DynamicGameObject
    DynamicGameObject <|-- RV
    RV *-- VehicleBody
    RV *-- ImpactDetector : breaks the windscreen, wrecks
    RV *-- EngineSimulator
    VehicleBody *-- VehicleParams
    note for VehicleParams "is VehicleBody::Params"
    VehicleParams *-- Wheel
    VehicleBody *-- WheelState
    VehicleBody ..> Surface : per wheel, from the floor
    RV ..> Stage : materialAt, for each wheel's Surface

    Stage --> FloorMode
    Stage *-- Body : bodies
    Stage *-- Cell : gridCells
    Body o-- GameObject
    Body o-- DynamicGameObject
    Stage ..> CollisionShape : collide, floorSamples
    DynamicGameObject ..> Stage : contactFloor reads floorAt
```

**One frame of physics** (detail in the [sequence diagram](#92-one-frame-of-physics)):

1. Every dynamic object advances (`update(dt)`) and the stage applies `apply()`, which calls `collideWithFloor`. If the object has its own floor logic (`contactFloor`), like the RV with its suspension, the `Stage` leaves it to the object.
2. **Shape against floor:** the lowest points of the shape (`floorSamples`) cannot end up under the floor. The object is pushed up with `applyCollision`.
3. **Shape against shape:** the `Stage` puts every dynamic object in the grid (8 m cells) and **only compares pairs sharing a cell**. A collision separates the objects: only the dynamic one moves against a static one, and between two dynamic ones the lighter moves more (`getMass`). The approach velocity is removed, with no bounce. Two passes. A sideways hit against a box whose top is within `getStepHeight()` of the feet (the `Walker`: 0.7 m) becomes an upward push, so the player climbs the RV's step.
4. The floor check is repeated, in case the push sank something into the terrain.

**Materials:** besides the height, the `Stage` says what the floor is made of (`materialAt`: sand, asphalt or grass), from a `MaterialMap` it receives with the floor (required with `HeightField`). Each RV wheel asks and receives a `Surface` (grip, rolling resistance, top speed): it is slower on sand.

**Floor:** `FloorMode::HeightField` interpolates a regular grid of heights (O(1), no overhangs). `FloorMode::DownwardRay` casts a vertical ray against the triangles, indexed in another grid (any mesh). It is chosen when the `Stage` is created.

**`VehicleBody`** (the RV) is a rigid body with its centre of mass below the axles. Each wheel hangs from a damped spring with limited travel; the tyre provides traction, braking and lateral grip. The chassis corners are elastic contact points with friction. A self-righting torque always returns it to its wheels. After the engine explodes (`setWrecked`) the wheels, tyres, engine and self-righting go away and the chassis falls and drags like a box.

**The RV is hollow.** Its collision is a `CompoundShape` (`hullShape()`): floor, roof, walls (the +x wall in four pieces around the door opening), the door (the only part that switches off when it opens), dashboard block, slanted windscreen, front and a step. The door is a hinged panel simulated with inertia: braking or turning hard swings it open, accelerating slams it shut.

## 5. Rendering

```mermaid
---
title: Rendering - meshes, models, animation, camera, light and effects
---
classDiagram
    direction LR

    class Shader {
        +use()
        +setInt(name, value)
        +setFloat(name, value)
        +setVector2(name, x, y)
        +setVector3(name, x, y, z)
        +setMatrix4(name, matrix)
    }
    class Mesh {
        -Vertex[] vertices
        -uint[] indices
        -Texture[] textures
        -float opacity
        +Draw(shader)
        +deformed(function) Mesh
    }
    class Model {
        +Mesh[] meshes
        +loadModel(path)
        +Draw(shader, translucent)
        +DrawTransparent(shader)
        +deformed(function) Model
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
        +setBoneGlobals(globals, orphansFollow)
        +setIdle(bool)
        +setGlowing(bool)
        +Draw(shader)
    }
    class Skeleton {
        +BoneInfo[] bones
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
        not used yet
        +string name
        +float start_time
        +float end_time
    }
    class ProceduralPose {
        <<abstract>>
        Poses generated by code
    }
    class PointRig {
        Bone rig from points
    }
    class ParticleEmitter {
        -vec3 position
        -float rate
        +setRate(perSecond)
        +setGround(function)
        +burst(count)
        +update(dt)
        +getParticles() Particle[]
    }
    class ParticleSettings {
        <<struct>>
        rate, cone, gravity, drag, fadeStart
    }
    class Particle {
        <<struct>>
        +vec3 position
        +vec3 velocity
        +float age
        +float life
    }
    class ParticleRenderer {
        +setLighting(...)
        +draw(emitters, camera)
    }
    class Explosion {
        Emitters and blast sound
    }
    class FilmGrain {
        Full-screen grain pass
        +draw(opacity, time)
    }
    class LineRenderer {
        Debug lines, no depth test
        +line(a, b, color)
        +box(min, max, color)
        +circle(centre, normal, radius, color)
        +draw(camera)
    }
    class Camera {
        -mat4 projection
        -mat4 view
        -float fov
        +resize()
        +setAspect(aspect)
        +setAngles(yaw, pitch)
        +setFov(degrees)
        +setFarPlane(distance)
        +setCarrier(object)
        +attachTo(target, distance, height)
        +follow()
        +getViewProjection() mat4
    }
    class Light {
        -vec3 color
        -vec3 position
        +moveTo(x, y, z)
        +setColor(r, g, b)
    }
    class SpotLight {
        <<struct>>
        position, direction, cone, colour
    }
    class RenderStats {
        <<struct>>
        Counters for --profile
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
    AnimatedModel ..> ProceduralPose : setBoneGlobals
    PointRig ..> ProceduralPose
    Camera --> Shader : writes projection, view, model
    ParticleEmitter "1" *-- "*" Particle
    ParticleEmitter --> ParticleSettings
    ParticleRenderer ..> ParticleEmitter : draws its particles
    ParticleRenderer ..> Camera : getViewProjection, getRight, getUp
    ParticleRenderer --> Shader : particle.vert / particle.frag
    Explosion *-- ParticleEmitter
    FilmGrain --> Shader : grain.vert / grain.frag
    LineRenderer ..> Camera : getViewProjection
    LineRenderer --> Shader : lines.vert / lines.frag
    Light --> Shader : writes lightPosition, lightColor
    SpotLight ..> Shader : spot* uniforms, MAX_SPOTS = 8
    Camera --> GameObject : target / carrier
    GameObject o-- Model : parts
    GameObject o-- AnimatedModel
```

`Shader` is the meeting point: the camera, the lights and every mesh write their uniforms into it. There is one program for the world; the uniform contract is in [ARCHITECTURE.md](ARCHITECTURE.md). `unlit` selects between lit (0), sky (1), emissive (2) and procedural sky (3) per part; `Environment::canopyMask` adds the forest-canopy shadow (texture unit 7).

**Mirrors.** While driving, `main` renders the scene a second time into a small FBO for each side mirror (320×640), one mirror per frame, using a second `Camera` placed on the glass. `GameStage::rearMirror(side)` returns a `MirrorView` and the `RV` shows the texture on its mirror glass (`RV::setMirrorTexture`). The centre mirror looks out of the rear window. Flatwoods only exists in these passes.

## 6. Input and interaction

```mermaid
---
title: Input and interaction
---
classDiagram
    direction LR

    class Controller {
        -float yaw
        -float pitch
        -bool enabled
        -bool lookEnabled
        +attach(character, distance, height)
        +setEnabled(enabled)
        +setLookEnabled(enabled)
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
        LeaveVehicle
        Headlights
        VehicleCamera
        Engine
        Handbrake
        ShipLegs
        Quit
        Maps
        DebugSelect
        DebugPlace
        DebugInspect
        Console
    }
    class Settings {
        +load() bool
        +save() bool
        +getFloat(key, fallback) float
        +setFloat(key, value)
        +getString(key, fallback) string
        +setString(key, value)
    }
    class Commands {
        Registered slash commands
        +add(name, function)
        +run(line)
    }
    class CommandConsole {
        T or /
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
        +update(playerPosition, enabled)
    }
    class Interactable {
        <<interface>>
        +getInteractionName() string
        +getInteractionPoint() vec3
        +getInteractionRange() float
        +isInteractionAvailable() bool
        +usesDirectly() bool
        +onUse(playerPosition)
        +buildInterface(panel)*
    }
    class UIManager {
        +open(Interactable) UIPanel
        +setHint(text)
        +hasPanels() bool
        +bindKey(key, action)
        +bindChar(char, action)
    }
    class UIPanel
    class DebugSelector {
        -Mode mode
        -weak_ptr~GameObject~ selected
        -weak_ptr~GameObject~ hovered
        -LineRenderer lines
        +toggleSelect()
        +togglePlace()
        +toggleInspect()
        +capturesMouse() bool
        +clear()
        +update(stage, camera, canPick)
        +draw(camera)
        +draw(renderer, width, height)
    }
    class UIOverlay {
        <<interface>>
        +draw(renderer, width, height)*
    }
    class GameObject {
        +describe(lines)
        +getProperties(properties)
    }
    class Property {
        <<struct>>
        +Kind kind
        +get() float
        +set(value)
        +valueText() string
    }
    class PropertyPanel {
        -shared_ptr~GameObject~ object
        -Property[] properties
    }
    class CollisionShape {
        +raycast(pose, origin, direction, distance)* bool
    }
    class Stage {
        +relocate(object, position)
        +turn(object, radians)
        +getProperties(properties)
    }

    Controller --> Camera : turns with the mouse
    Controller --> Controls : which key is each action
    Controller --> PlayableCharacter : control(dir, up, yaw)
    Controls --> Action
    Controls ..> Settings : readFrom / writeTo
    CommandConsole --|> UIPanel
    CommandConsole --> Commands : run
    InteractionSystem --> Controls : use key
    InteractionSystem --> Interactable : the closest, available and in range
    InteractionSystem --> UIManager : opens its panel (or calls onUse)
    UIManager ..> Interactable : open
    Interactable ..> UIPanel : buildInterface
    UIOverlay <|.. DebugSelector
    DebugSelector --> UIManager : addOverlay / removeOverlay
    DebugSelector --> Controls : name of its key
    DebugSelector *-- LineRenderer
    DebugSelector ..> CollisionShape : raycast from the camera
    DebugSelector --> GameObject : the chosen / aimed one (weak_ptr)
    DebugSelector ..> Stage : relocate, turn, world properties
    DebugSelector ..> PropertyPanel : click in properties mode
    UIPanel <|-- PropertyPanel
    PropertyPanel *-- Property
    GameObject ..> Property : getProperties
    Stage ..> Property : getProperties (the World)
    Controller ..> DebugSelector : main turns the view off if capturesMouse
```

The `main` loop applies one rule: `controller.setEnabled(!ui.hasPanels() && !isPlayerDead())`. With any panel open the controls pause and the cursor is free; the world itself is **never** paused. While paralysed (`GameStage::playerImmobilized`) `main` also stops the `Controller`. `InteractionSystem` only opens a panel when none is open, and with `enabled = false` (while driving) it neither offers nor uses anything.

**Vehicle keys.** Left Shift (`Action::LeaveVehicle`) does three things: run on foot, get out of the vehicle being driven (only below 2 m/s for the RV) and, if Bob or Flatwoods hold you, break free (`leaveVehicle` → `Bob::struggleOnce` / end of possession). R (`Engine`) turns the key and starts an engine attempt, Space (`Handbrake`) pulls the handbrake (or rises in the saucer), F (`Headlights`) is the flashlight, the vehicle lights or the saucer's ray gun, Q (`ShipLegs`) retracts the saucer's legs.

**Debug modes.** `Action::DebugSelect` (key 1), `DebugPlace` (2) and `DebugInspect` (0) switch the `DebugSelector` modes. In placement mode the cross marks a floor point (`floorHit`) and a click moves the chosen object with `Stage::relocate` (which calls the virtual `teleport` and re-registers static objects in the grid); with the right button the mouse turns it (`Stage::turn`, in 15° steps with Shift). In properties mode a click opens a `PropertyPanel` with the `Property` list of the aimed object; aiming at nothing edits the World's (time, speed, day length). It is a `UIOverlay`, not a panel, so it does not pause the controls: left click casts a ray from the camera (`CollisionShape::raycast`) and the right one chooses the player. It only keeps a `weak_ptr` and `switchMap` empties it with `clear()`.

## 7. Audio and dialogue

```mermaid
---
title: Audio, voice and dialogue
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
        Silent: letter by letter
        +say(text)
        +progress() float
    }
    class SpeechSynthesizer {
        <<interface>>
        +synthesize(text, settings) AudioClip
    }
    class EspeakSynthesizer {
        Launches espeak-ng
    }
    class VoiceSettings {
        <<struct>>
        +string language
        +int wordsPerMinute
        +int pitch
    }
    class SoundEngine {
        +isAvailable() bool
        +play(clip, spatial, position, loop, channel) Sound
        +playGenerated(generator, channel) Sound
        +setListener(position, forward)
        +setVolume(channel, volume)
    }
    class Channel {
        <<enumeration>>
        Game
        Music
    }
    class Sound {
        +isPlaying() bool
        +setPosition(position)
        +setVolume(volume)
        +setDistances(min, max)
        +setLooping(looping)
        +stop()
    }
    class MusicPlayer {
        -Sound sound
        +play(clip, loop, volume)
        +stop()
    }
    class AudioGenerator {
        <<interface>>
        Procedural audio, filled in real time
        +fill(samples, count)*
    }
    class EngineSynth {
        90s V8
    }
    class EngineSound {
        Start clip + synth, driven by EngineSimulator
    }
    class BuzzSynth {
        Mosquito buzz
    }
    class Stage {
        <<abstract>>
        -AudioClip music
        +setMusic(clip, loop, volume)
        +loadMusic(path) bool
    }
    class AudioClip {
        <<struct>>
        +float[] samples
        +uint channels
        +uint sampleRate
        +loadWavFile(path) bool
    }
    class Npc
    class Readable
    class UIPanel
    class UITextBlock

    LineNarrator <|.. Voice
    LineNarrator <|.. Typewriter
    SpeechSynthesizer <|-- EspeakSynthesizer
    AudioGenerator <|-- EngineSynth
    AudioGenerator <|-- BuzzSynth
    EngineSound *-- EngineSynth
    Dialogue --> LineNarrator : hands over each line
    Dialogue ..> UIPanel : buildPanel
    Dialogue ..> UITextBlock : subtitles with progress()
    Voice --> SoundEngine
    Voice --> SpeechSynthesizer : synthesises in the background
    Voice *-- Sound
    Voice --> VoiceSettings
    SpeechSynthesizer ..> AudioClip : returns
    SoundEngine ..> Sound : play() returns it
    SoundEngine ..> Channel
    SoundEngine ..> AudioGenerator : playGenerated
    Sound o-- AudioClip : clip
    MusicPlayer --> SoundEngine
    MusicPlayer *-- Sound : the current track
    Stage o-- AudioClip : music, null = none
    MusicPlayer ..> Stage : switchMap hands over its music
    Npc *-- Voice
    Npc *-- Dialogue
    Readable *-- Typewriter
    Readable *-- Dialogue
```

The **background music** belongs to the `Stage` (`music`, `nullptr` = none, looping by default) and is played by the `MusicPlayer` on the `Music` channel; `switchMap` hands it over when the map changes. Everything else (ambient wind, creatures, engine, the saucer's hum, steam and ray gun) is on the `Game` channel, and the two have separate volumes (`audio.music_volume`, `audio.game_volume`).

`Dialogue` does not know whether text is spoken or read: it delegates to a `LineNarrator`. `Voice` (spoken, for `Npc`s) and `Typewriter` (silent, for `Readable`s) implement it, and `progress()` makes the subtitles advance at the pace of delivery. The `SoundEngine` listener follows the camera every frame. Procedural sounds (`EngineSynth`, `BuzzSynth`) implement `AudioGenerator` and are played with `playGenerated`.

## 8. User interface and menus

```mermaid
---
title: User interface and menus
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
    class UITextField
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

    class UIOverlay {
        <<interface>>
        +draw(renderer, width, height)*
    }
    class DeathOverlay {
        Red, then white or black
        +setColor(color)
    }
    class CreditsOverlay {
        Reads assets/credits/credits.txt
    }
    class StruggleOverlay {
        Mash-Shift prompt + bar
    }
    class CrosshairOverlay {
        Saucer gun sight
    }
    class UIManager {
        -UIPanel[] panels
        -UIOverlay[] overlays
        -UIRenderer renderer
        -Binding[] bindings
        +open(panel) UIPanel
        +open(Interactable) UIPanel
        +close(panel)
        +closeAll()
        +bindKey(key, action)
        +setHint(text)
        +addOverlay(overlay)
        +removeOverlay(overlay)
        +update()
        +draw()
    }
    class UIRenderer {
        Own shader, restores the previous one
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
        +SoundEngine sound
        +function quit
    }

    class PauseMenu
    class OptionsMenu
    class SettingsMenu {
        <<abstract>>
        Defaults / Save / Back
        +hasUnsavedChanges() bool*
        +apply() bool*
        +discard()*
        +resetToDefaults()*
        +save()
        +leave()
    }
    class CameraMenu {
        Sensitivity and FOV
    }
    class ControlsMenu {
        Rebinds keys on a copy
    }
    class AudioMenu {
        Music and game volume
        +applySettings(settings, sound)$
        +storeSettings(settings, sound)$
    }
    class ConfirmDialog {
        Question with several answers
    }
    class CommandConsole {
        Slash commands
    }
    class MapSelector {
        Debug, key Z
    }
    class SoundEngine
    class Interactable {
        <<interface>>
    }
    class Shader

    UIElement <|-- UILabel
    UIElement <|-- UIButton
    UIElement <|-- UISlider
    UIElement <|-- UIInfoRow
    UIElement <|-- UITextBlock
    UIElement <|-- UITextField
    UIElement <|-- UIContainer
    UIContainer <|-- UIPanel
    UIContainer <|-- UIRow
    UIContainer "1" *-- "*" UIElement : children

    UIPanel <|-- PauseMenu
    UIPanel <|-- OptionsMenu
    UIPanel <|-- SettingsMenu
    UIPanel <|-- ConfirmDialog
    UIPanel <|-- MapSelector
    UIPanel <|-- CommandConsole
    SettingsMenu <|-- CameraMenu
    SettingsMenu <|-- ControlsMenu
    SettingsMenu <|-- AudioMenu
    AudioMenu ..> SoundEngine : volume per channel

    UIOverlay <|.. DeathOverlay
    UIOverlay <|.. CreditsOverlay
    UIOverlay <|.. StruggleOverlay
    UIOverlay <|.. CrosshairOverlay

    UIManager "1" *-- "*" UIPanel : panels, the last on top
    UIManager "1" o-- "*" UIOverlay : overlays, under the panels
    UIManager *-- UIRenderer
    UIRenderer --> Shader : ui.vert, ui.frag
    UIManager ..> Interactable : open
    PauseMenu ..> MenuContext
    OptionsMenu ..> MenuContext
    SettingsMenu ..> MenuContext
    PauseMenu ..> OptionsMenu : opens
    OptionsMenu ..> CameraMenu : opens
    OptionsMenu ..> ControlsMenu : opens
    OptionsMenu ..> AudioMenu : opens
    SettingsMenu ..> ConfirmDialog : unsaved changes
```

`UIManager` is the **only owner** of GLFW's keyboard and character callbacks and of the window's user pointer. It hands keys to the top panel (`onKey`); Esc closes that panel and, with none open, runs the shortcut bound with `bindKey`/`bindChar` (Esc → `PauseMenu`, Z → `MapSelector`, T or `/` → `CommandConsole`). All menus are `UIPanel`s and receive a `MenuContext` with what they need. The death, credits, struggle and crosshair screens are overlays that `main` shows on demand (death → fade to black → credits, also after an abduction).

## 9. Sequences

### 9.1 One frame

```mermaid
---
title: One frame of the main loop
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
    participant R as ParticleRenderer
    participant D as DebugSelector

    M->>M: dt = time since the previous frame
    opt a map change is pending
        M->>M: switchMap() (deferred, outside the UI)
        Note over M: starts the new map's music (MusicPlayer)
    end
    M->>K: resize()
    M->>I: update(player position, interactionsEnabled)
    I->>U: setHint() / open(Interactable)
    M->>U: update()
    U-->>M: hasPanels()
    opt the map changed the player (RV, saucer, on foot)
        M->>S: takePlayerChange()
        M->>C: attach(new player, distance, height, yaw)
    end
    M->>C: setEnabled(!hasPanels and alive and not paralysed)
    M->>C: setLookEnabled(!selector.capturesMouse())
    M->>C: update()
    C->>K: setAngles(yaw, pitch)
    C->>P: control(dir, up, yaw)
    M->>S: update(dt)
    Note over S: see 9.2: move, floor, collisions
    M->>P: followCamera()
    P->>K: follow()
    M->>D: update(stage, camera, !hasPanels)
    Note over D: click chooses (1), relocates (2) or edits (0)
    opt driving the RV
        M->>S: rearMirror(side) for one mirror
        M->>S: render(...) into the mirror texture
    end
    M->>S: render(shader, camera, time)
    M->>R: draw(map emitters, camera)
    M->>D: draw(camera): shape and AABB of the chosen one
    M->>U: draw()
    U->>D: draw(renderer, width, height): cross and data
```

### 9.2 One frame of physics

```mermaid
---
title: Stage.update - move, floor and collisions
---
sequenceDiagram
    autonumber
    participant S as Stage (GameStage)
    participant O as GameObject (static)
    participant D as DynamicGameObject
    participant R as RV
    participant V as VehicleBody
    participant X as CollisionShape

    S->>O: update(dt) of each one
    loop each dynamic object
        S->>D: update(dt)
        S->>S: apply(object, dt) → collideWithFloor
        alt the object has its own floor logic (RV)
            S->>R: contactFloor(stage, dt)
            R->>V: step(dt, floor query)
            V->>S: floorAt(x, z) at each wheel and corner
            S-->>V: height and normal
            V-->>R: position, orientation, suspension
            R->>S: keepInsideFloor()
        else the rest
            S->>S: fits to the floor (gravity, snap)
        end
        S->>X: floorSamples(pose)
        S->>S: floorAt() of each point
        S->>D: applyCollision(upward push)
    end
    S->>S: resolveCollisions()
    Note over S: puts the dynamics in the grid and only compares pairs in the same cell
    loop pairs sharing a cell, 2 passes
        S->>X: collide(A, poseA, B, poseB)
        X-->>S: normal and depth
        Note over S: a side hit on a low enough box becomes a step up (getStepHeight)
        S->>D: applyCollision(push, velocity change)
    end
    loop each dynamic object
        S->>S: shape against floor again
    end
```

### 9.3 Talking to an NPC

```mermaid
---
title: Pressing E in front of an NPC
---
sequenceDiagram
    autonumber
    actor J as Player
    participant I as InteractionSystem
    participant U as UIManager
    participant N as Npc
    participant D as Dialogue
    participant V as Voice
    participant E as EspeakSynthesizer
    participant A as SoundEngine

    J->>I: presses E (Action::Use)
    I->>I: picks the closest Interactable in range
    I->>U: open(npc)
    U->>N: buildInterface(panel)
    N->>D: buildPanel(panel)
    U-->>I: panel
    I->>N: onInterfaceOpened(player position)
    N->>N: faceTowards(player)
    N->>D: start()
    D->>V: say(line)
    V->>E: synthesize(text) in the background
    E-->>V: AudioClip
    V->>A: plays the clip in 3D, at the NPC's mouth
    loop while speaking
        D->>V: progress()
        Note over D: the subtitles advance with the audio
    end
    J->>D: Next / Close / Esc
    D->>V: stop()
    I->>N: onInterfaceClosed()
```

### 9.4 Getting in and out of the RV

```mermaid
---
title: Enter the RV on foot, start driving, and get out with Shift
---
sequenceDiagram
    autonumber
    actor J as Player
    participant I as InteractionSystem
    participant R as RV (door)
    participant W as RV::Steering
    participant T as VehicleStage (GameStage)
    participant K as Walker
    participant M as main (loop)
    participant C as Controller
    participant U as UIManager

    Note over J,K: starts on foot, first person
    J->>I: E next to the door (available and in range)
    I->>R: usesDirectly() → true
    I->>R: onUse(position)
    R->>R: toggleDoor() (a push; the door then swings with inertia)
    J->>K: walks in over the step (getStepHeight) and through the hollow cabin
    J->>I: E next to the steering wheel (inside, ≤ 0.9 m from the seat)
    I->>W: onUse(position)
    W->>T: enterAction → enterRV()
    T->>K: hidden, no collision or gravity
    T->>R: setOccupied(true)
    T->>T: setPlayer(rv, distance, height, yaw)
    M->>T: takePlayerChange()
    T-->>M: true
    M->>C: attach(rv, distance, height, yaw)
    C->>R: control(...) every frame, cockpit camera follows the RV
    J->>U: Left Shift (LeaveVehicle, no panels)
    U->>T: leaveVehicle() (only below 2 m/s)
    T->>R: control(0) and setOccupied(false)
    Note over R: the handbrake is not applied by itself; the engine stays on
    T->>K: next to the steering wheel, inside, with collision and gravity
    T->>T: setPlayer(walker, 0, 1.6, yaw forward)
    M->>T: takePlayerChange()
    M->>C: attach(walker, 0, 1.6, yaw)
    Note over J,K: on foot again, inside the RV (a SafeSpace)
```

## 10. Index of all classes

Built from the headers in `src/`. The last column is the section where the class appears (a `—` in *Inherits from* means no base class). Helper types that live inside another class (`Part`, `VehicleBody::Params`, `Stage::Body`, `RV::Steering`…) show in their class's diagram.

| Class | File | Inherits from | Section |
|---|---|---|---|
| `Bob` | `src/entities/Bob.h` | `DynamicGameObject` | 3 |
| `AlienVisit` | `src/entities/AlienVisit.h` | — | 3 |
| `Saucer` | `src/entities/Saucer.h` | `PlayableCharacter`, `Interactable` | 3 |
| `Flatwoods` | `src/entities/Flatwoods.h` | `DynamicGameObject` | 3 |
| `FollaCulos` | `src/entities/FollaCulos.h` | `Npc` | 3 |
| `Mosquito` | `src/entities/Mosquito.h` | `DynamicGameObject` | 3 |
| `MosquitoEgg` | `src/entities/MosquitoEgg.h` | `DynamicGameObject` | 3 |
| `NpcRagdoll` | `src/entities/NpcRagdoll.h` | — | 3 |
| `Npc` | `src/entities/Npc.h` | `DynamicGameObject`, `Interactable` | 2 |
| `Pingu` | `src/entities/Pingu.h` | `Npc` | 2 |
| `RV` | `src/entities/RV.h` | `PlayableCharacter`, `Interactable` | 2 |
| `Readable` | `src/entities/Readable.h` | `GameObject`, `Interactable` | 2 |
| `Satellite` | `src/entities/Satellite.h` | `GameObject`, `Interactable` | 2 |
| `FuelPump` | `src/entities/FuelPump.h` | `GameObject`, `Interactable` | 2 |
| `Walker` | `src/entities/Walker.h` | `PlayableCharacter` | 2 |
| `TestStage` | `src/test.cpp` | `VehicleStage` | 2 |
| `ForestStage` | `src/world/ForestStage.h` | `VehicleStage` | 2 |
| `PineForestStage` | `src/world/PineForestStage.h` | `VehicleStage` | 2 |
| `Route66Stage` | `src/world/Route66Stage.h` | `VehicleStage` | 2 |
| `VehicleStage` | `src/world/VehicleStage.h` | `GameStage` | 2 |
| `DynamicGameObject` | `src/world/DynamicGameObject.h` | `GameObject` | 2 |
| `GameObject` | `src/world/GameObject.h` | — | 2 |
| `Environment` | `src/world/GameStage.h` | — | 2 |
| `GameStage` | `src/world/GameStage.h` | `Stage` | 2 |
| `Interactable` | `src/world/Interactable.h` | — | 2 |
| `PlayableCharacter` | `src/world/PlayableCharacter.h` | `DynamicGameObject` | 2 |
| `FloorMaterial` | `src/world/FloorMaterial.h` | — | 2 |
| `MaterialMap` | `src/world/MaterialMap.h` | — | 2 |
| `MirrorView` | `src/world/MirrorView.h` | — | 5 |
| `SafeSpace` | `src/world/SafeSpace.h` | — | 2 |
| `Effect` | `src/world/SceneFile.h` | — | 2 |
| `SceneFile` | `src/world/SceneFile.h` | — | 2 |
| `SceneObject` | `src/world/SceneFile.h` | — | 2 |
| `SceneStage` | `src/world/SceneStage.h` | `GameStage` | 2 |
| `FloorMode` | `src/world/Stage.h` | — | 2 |
| `Stage` | `src/world/Stage.h` | — | 2 |
| `Property` | `src/world/Property.h` | — | 6 |
| `Box` | `src/physics/CollisionShape.h` | `CollisionShape` | 4 |
| `Capsule` | `src/physics/CollisionShape.h` | `CollisionShape` | 4 |
| `CompoundShape` | `src/physics/CollisionShape.h` | `CollisionShape` | 4 |
| `CollisionShape` | `src/physics/CollisionShape.h` | — | 4 |
| `Contact` | `src/physics/CollisionShape.h` | — | 4 |
| `Pose` | `src/physics/CollisionShape.h` | — | 4 |
| `EngineSimulator` | `src/physics/EngineSimulator.h` | — | 4 |
| `ImpactDetector` | `src/physics/ImpactDetector.h` | — | 4 |
| `VehicleBody` | `src/physics/VehicleBody.h` | — | 4 |
| `Ragdoll` | `src/physics/Ragdoll.h` | `ProceduralPose` | 3 |
| `SpiderGait` | `src/physics/SpiderGait.h` | `ProceduralPose` | 3 |
| `InsectLegs` | `src/physics/InsectLegs.h` | `ProceduralPose` | 3 |
| `ProceduralPose` | `src/render/ProceduralPose.h` | — | 3 |
| `PointRig` | `src/render/PointRig.h` | — | 5 |
| `AnimatedMesh` | `src/render/AnimatedMesh.h` | — | 5 |
| `AnimatedVertex` | `src/render/AnimatedMesh.h` | — | 5 |
| `AnimatedModel` | `src/render/AnimatedModel.h` | — | 5 |
| `Animation` | `src/render/Animation.h` | — | 5 |
| `Camera` | `src/render/Camera.h` | — | 5 |
| `Light` | `src/render/Light.h` | — | 5 |
| `SpotLight` | `src/render/SpotLight.h` | — | 5 |
| `Mesh` | `src/render/Mesh.h` | — | 5 |
| `Texture` | `src/render/Mesh.h` | — | 5 |
| `Vertex` | `src/render/Mesh.h` | — | 5 |
| `Model` | `src/render/Model.h` | — | 5 |
| `Shader` | `src/render/Shader.h` | — | 5 |
| `LineRenderer` | `src/render/LineRenderer.h` | — | 5 |
| `BoneInfo` | `src/render/Skeleton.h` | — | 5 |
| `Skeleton` | `src/render/Skeleton.h` | — | 5 |
| `ParticleEmitter` | `src/effects/ParticleEmitter.h` | — | 5 |
| `ParticleRenderer` | `src/effects/ParticleRenderer.h` | — | 5 |
| `Explosion` | `src/effects/Explosion.h` | — | 5 |
| `FilmGrain` | `src/effects/FilmGrain.h` | — | 5 |
| `RenderStats` | `src/core/RenderStats.h` | — | 5 |
| `Settings` | `src/core/Settings.h` | — | 6 |
| `Commands` | `src/core/Commands.h` | — | 6 |
| `PoissonDisk` | `src/core/PoissonDisk.h` | — | — |
| `Controller` | `src/input/Controller.h` | — | 6 |
| `Action` | `src/input/Controls.h` | — | 6 |
| `Controls` | `src/input/Controls.h` | — | 6 |
| `InteractionSystem` | `src/input/InteractionSystem.h` | — | 6 |
| `DebugSelector` | `src/debug/DebugSelector.h` | `UIOverlay` | 6 |
| `PropertyPanel` | `src/debug/PropertyPanel.h` | `UIPanel` | 6 |
| `AudioClip` | `src/audio/AudioClip.h` | — | 7 |
| `AudioGenerator` | `src/audio/AudioGenerator.h` | — | 7 |
| `BuzzSynth` | `src/audio/BuzzSynth.h` | `AudioGenerator` | 7 |
| `EngineSound` | `src/audio/EngineSound.h` | — | 7 |
| `EngineSynth` | `src/audio/EngineSynth.h` | `AudioGenerator` | 7 |
| `EspeakSynthesizer` | `src/audio/EspeakSynthesizer.h` | `SpeechSynthesizer` | 7 |
| `MusicPlayer` | `src/audio/MusicPlayer.h` | — | 7 |
| `Sound` | `src/audio/SoundEngine.h` | — | 7 |
| `SoundEngine` | `src/audio/SoundEngine.h` | — | 7 |
| `SpeechSynthesizer` | `src/audio/SpeechSynthesizer.h` | — | 7 |
| `VoiceSettings` | `src/audio/SpeechSynthesizer.h` | — | 7 |
| `Voice` | `src/audio/Voice.h` | `LineNarrator` | 7 |
| `Dialogue` | `src/dialogue/Dialogue.h` | — | 7 |
| `LineNarrator` | `src/dialogue/LineNarrator.h` | — | 7 |
| `Typewriter` | `src/dialogue/Typewriter.h` | `LineNarrator` | 7 |
| `AudioMenu` | `src/ui/AudioMenu.h` | `SettingsMenu` | 8 |
| `CameraMenu` | `src/ui/CameraMenu.h` | `SettingsMenu` | 8 |
| `CommandConsole` | `src/ui/CommandConsole.h` | `UIPanel` | 8 |
| `ConfirmDialog` | `src/ui/ConfirmDialog.h` | `UIPanel` | 8 |
| `ControlsMenu` | `src/ui/ControlsMenu.h` | `SettingsMenu` | 8 |
| `CreditsOverlay` | `src/ui/CreditsOverlay.h` | `UIOverlay` | 8 |
| `CrosshairOverlay` | `src/ui/CrosshairOverlay.h` | `UIOverlay` | 8 |
| `DeathOverlay` | `src/ui/DeathOverlay.h` | `UIOverlay` | 8 |
| `MapSelector` | `src/ui/MapSelector.h` | `UIPanel` | 8 |
| `MenuContext` | `src/ui/MenuContext.h` | — | 8 |
| `OptionsMenu` | `src/ui/OptionsMenu.h` | `UIPanel` | 8 |
| `PauseMenu` | `src/ui/PauseMenu.h` | `UIPanel` | 8 |
| `SettingsMenu` | `src/ui/SettingsMenu.h` | `UIPanel` | 8 |
| `StruggleOverlay` | `src/ui/StruggleOverlay.h` | `UIOverlay` | 8 |
| `UIButton` | `src/ui/UIButton.h` | `UIElement` | 8 |
| `UIContainer` | `src/ui/UIContainer.h` | `UIElement` | 8 |
| `UIElement` | `src/ui/UIElement.h` | — | 8 |
| `UIInfoRow` | `src/ui/UIInfoRow.h` | `UIElement` | 8 |
| `UILabel` | `src/ui/UILabel.h` | `UIElement` | 8 |
| `UIManager` | `src/ui/UIManager.h` | — | 8 |
| `UIOverlay` | `src/ui/UIOverlay.h` | — | 8 |
| `UIPanel` | `src/ui/UIPanel.h` | `UIContainer` | 8 |
| `UIRenderer` | `src/ui/UIRenderer.h` | — | 8 |
| `UIRow` | `src/ui/UIRow.h` | `UIContainer` | 8 |
| `UISlider` | `src/ui/UISlider.h` | `UIElement` | 8 |
| `UITextBlock` | `src/ui/UITextBlock.h` | `UIElement` | 8 |
| `UITextField` | `src/ui/UITextField.h` | `UIElement` | 8 |

## How to maintain this document

- If you add a class, put it in the diagram of its subsystem (with its relations; there is no need to list every member) and in the index.
- If you change an inheritance, an owner (`*--`) or an important dependency, update the arrow. This is what drifts most.
- The PNGs in `docs/diagrams/` are copies of the Mermaid blocks and need regenerating when a diagram changes (`mmdc -p cfg.json -i block.mmd -o image.png -s 2`; see `CLAUDE.md`). They are currently stale.
- The diagrams are text: edit them by hand and GitHub renders them. To check that the syntax is valid before pushing, pass each block through Mermaid's parser (`mermaid.parse`).
