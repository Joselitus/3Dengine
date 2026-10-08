#include <GL/glew.h>
#include <climits>
#include <cstdlib>
#include <unistd.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "AnimatedMesh.h"
#include "AnimatedModel.h"
#include "Camera.h"
#include "Controller.h"
#include "Controls.h"
#include "CommandConsole.h"
#include "Commands.h"
#include "TextFormat.h"
#include "DebugSelector.h"
#include "CreditsOverlay.h"
#include "DeathOverlay.h"
#include "FilmGrain.h"
#include "StruggleOverlay.h"
#include "EspeakSynthesizer.h"
#include "GameStage.h"
#include "GameObject.h"
#include "InteractionSystem.h"
#include "Light.h"
#include "MapSelector.h"
#include "PlayableCharacter.h"
#include "RV.h"
#include "Readable.h"
#include "RenderStats.h"
#include "Satellite.h"
#include "Model.h"
#include "FollaCulos.h"
#include "ForestStage.h"
#include "PineForestStage.h"
#include "Mosquito.h"
#include "MosquitoEgg.h"
#include "Pingu.h"
#include "AudioMenu.h"
#include "CameraMenu.h"
#include "PauseMenu.h"
#include "Route66Stage.h"
#include "SceneStage.h"
#include "Settings.h"
#include "Shader.h"
#include "Stage.h"
#include "Skeleton.h"
#include "MusicPlayer.h"
#include "ParticleRenderer.h"
#include "SoundEngine.h"
#include "UIManager.h"
#include "VehicleStage.h"
#include "Walker.h"
#include "myopengl.h"

using namespace std;
using namespace glm;

bool FULLSCREEN = true; // pass --windowed to disable
#define HEIGHT 600
#define WIDTH 700

// Global variables

const char *WINDOWNAME = "test bimbow";

// Vertex shader
const char *vertexShaderFile = "shaders/animatedshader.vert";

// Fragment shader
const char *fragmentShaderFile = "shaders/shader.frag";

GLFWwindow *initializeGLFW(const char *windowname) {
  // Initialise GLFW
  glewExperimental = true; // Needed for core profile

#ifdef GLFW_PLATFORM // GLFW >= 3.4 only
  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return NULL;
  }

  glfwWindowHint(GLFW_SAMPLES, 4);               // 4x antialiasing
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 3.3
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

  // Open a window and create its OpenGL context
  GLFWwindow *window; // (In the accompanying source code, this variable is
                      // global for simplicity)
  if (FULLSCREEN) {
    const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    // Ask for the current mode exactly: otherwise GLFW picks the highest
    // refresh rate and switches video mode, which blanks every screen.
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    int window_width = mode->width;
    int window_height = mode->height;
    window = glfwCreateWindow(window_width, window_height, windowname,
                              glfwGetPrimaryMonitor(), NULL);

  } else
    window = glfwCreateWindow(WIDTH, HEIGHT, windowname, NULL, NULL);

  if (window == NULL) {
    fprintf(stderr,
            "Failed to open GLFW window. If you have an Intel GPU, they are "
            "not 3.3 compatible. Try the 2.1 version of the tutorials.\n");
    glfwTerminate();
    return NULL;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync

  // Initialize GLEW
  glewExperimental = true; // Needed in core profile
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    fprintf(stderr, "Failed to initialize GLEW: %s\n", glewGetErrorString(err));
    return NULL;
  }

  // Ensure we can capture the escape key being pressed below
  glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

  // Disable cursor
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  return window;
}

// The desert: dunes with a road winding through them, cacti and rocks, an RV,
// a satellite the player can orient, a sign, an NPC and the player: a penguin
// on foot, in first person. Using the RV's door (Use key) puts the penguin
// inside it and hands the controls and the camera (third person) to the RV;
// the leave-vehicle key (Left Shift) puts the penguin on foot at the door and
// goes back to first person. The rest (the RV, the day cycle, the controls)
// is VehicleStage's.
class TestStage : public VehicleStage {
private:
  // The giant mosquitoes: the first one and those born of its eggs, the eggs waiting to hatch, the
  // pool they are laid in, and how many there may be at most (eggs included)
  std::vector<std::shared_ptr<Mosquito>> mosquitoes;
  int eggsWaiting = 0;
  vec3 pool = vec3(0.0f);
  static constexpr int MAX_MOSQUITOES = 8;
  SoundEngine *soundEngine = nullptr;
  // The places round the player's head where young mosquitoes bite (shared by all of them)
  std::shared_ptr<Mosquito::BiteSlots> biteSlots = std::make_shared<Mosquito::BiteSlots>();

  // A mosquito at `where`, roaming round `home`, `growth` grown (0 = just hatched). `later`: while
  // the stage is updating (a hatching egg), it joins at the end of the frame
  std::shared_ptr<Mosquito> addMosquito(const vec3 &where, const vec3 &home, float growth, bool later) {
    std::vector<std::shared_ptr<Model>> legSegments;
    for (int pair = 0; pair < 3; pair++)
      for (const char *side : {"L", "R"})
        for (int segment = 0; segment < 4; segment++)
          legSegments.push_back(loadModel("../assets/mosquito/mosquito_leg_" + std::string(side) +
                                          std::to_string(pair) + "_" + std::to_string(segment) + ".obj"));
    auto mosquito = make_shared<Mosquito>(loadModel("../assets/mosquito/mosquito.obj"),
                                          loadModel("../assets/mosquito/mosquito_abdomen.obj"),
                                          loadModel("../assets/mosquito/mosquito_wing_l.obj"),
                                          loadModel("../assets/mosquito/mosquito_wing_r.obj"),
                                          legSegments, *soundEngine);
    mosquito->setGrowth(growth);
    mosquito->setWaterSpots({pool});
    // It sucks the RV's fuel while the player is away from it, and bursts its tyres now and then
    // while he drives (and blows up doing it)
    mosquito->setVehicle(rv.get());
    mosquito->setHourQuery([this]() { return getTimeOfDay(); });
    mosquito->setHome(home);
    mosquito->setPosition(where.x, where.y, where.z);
    mosquito->setFloorQuery([this](float x, float z, float &height) { return floorAt(x, z, height); });
    mosquito->setTarget([this]() { return player->getPosition(); });
    mosquito->setPlayerInVehicleQuery([this]() { return inVehicle; });
    mosquito->setPlayerCaughtCallback([this]() { killPlayer(); });
    mosquito->setPlayerDeadQuery([this]() { return isPlayerDead(); });
    mosquito->setEggLayer([this](const vec3 &at) { return layEgg(at); });
    mosquito->setBiteSlots(biteSlots); // (several can bite the player at once)
    for (const auto &emitter : mosquito->getEmitters())
      addEmitter(emitter);
    mosquitoes.push_back(mosquito);
    if (later)
      addDynamicLater(mosquito);
    else
      addDynamic(mosquito);
    return mosquito;
  }

  // An egg floating on the water at `at`; in a few seconds it hatches into a young mosquito. False
  // if there are as many mosquitoes and eggs as there may be
  bool layEgg(const vec3 &at) {
    int alive = eggsWaiting;
    for (const auto &m : mosquitoes)
      if (!m->isDead())
        alive++;
    if (alive >= MAX_MOSQUITOES)
      return false;
    eggsWaiting++;
    auto egg = make_shared<MosquitoEgg>(loadModel("../assets/mosquito/mosquito_egg.obj"), [this](MosquitoEgg &e) {
      eggsWaiting--;
      vec3 p = e.getPosition();
      addMosquito(p + vec3(0.0f, Mosquito::MIN_CLEARANCE * Mosquito::BABY_SCALE + 0.05f, 0.0f), p, 0.0f, true);
      removeLater(&e);
    });
    egg->setPosition(at.x, pool.y + 0.04f, at.z);
    egg->setYaw((float)std::fmod(at.x * 12.9898f + at.z * 78.233f, 6.2831853f)); // (any way round)
    addDynamicLater(egg);
    return true;
  }
  static constexpr float GROUND_Y = -1.0f; // ground level of the clearing
  // What is within this many metres of the edge of the terrain is not drawn
  static constexpr float EDGE_CULL_MARGIN = 12.0f;

public:
  void getSpotLights(std::vector<SpotLight> &lights) const override {
    VehicleStage::getSpotLights(lights);
    for (const auto &mosquito : mosquitoes) // the flash of an explosion (only for a moment)
      mosquito->getLight(lights);
  }

  // The NPCs speak through `sound` with voices made by `speech`
  TestStage(FloorMode mode, SoundEngine &sound, SpeechSynthesizer &speech)
      : VehicleStage(mode) {
    soundEngine = &sound;
    groundFallback = GROUND_Y;
    // The desert's background music: an arid guitar and banjo loop
    loadMusic("../assets/music/desert.wav");
    // ...and the wind, which never stops (it is all that is left at night)
    loadAmbience("../assets/music/wind.wav");

    startDay();
    setEdgeCulling(EDGE_CULL_MARGIN); // (once the floor is known)
    // First person: the camera at the penguin's eyes, 1.6 above its feet
    cameraDistance = 0.0f;
    cameraHeight = EYE_HEIGHT;

    // Desert scenery
    auto ground = make_shared<GameObject>(loadModel("../assets/desert/dunes_loop.obj"));
    ground->setPosition(0.0f, GROUND_Y, 0.0f);
    ground->setCollidable(false); // it is the floor, not an obstacle
    add(ground);
    // The dunes are a regular grid of heights, so they are the height map
    // and a height field needs a material map: asphalt where the road is, sand
    // everywhere else (the RV is slower on sand)
    auto materials =
        MaterialMap::loadImage("../assets/desert/dunes_loop_materials.png");
    if (!materials) {
      fprintf(stderr, "No material map: the whole floor is sand\n");
      materials = MaterialMap::uniform(FloorMaterial::Sand);
    }
    setFloor(loadModel("../assets/desert/dunes_loop.obj"),
             vec3(0.0f, GROUND_Y, 0.0f), materials);
    // The road: 8 m wide, a closed loop winding round the starting clearing
    // (about 250 m long), carved into the dunes
    auto road = make_shared<GameObject>(loadModel("../assets/desert/road.obj"));
    road->setPosition(0.0f, GROUND_Y, 0.0f);
    road->setCollidable(false);
    add(road);

    // model, x, z, rotation around y, uniform scale. Scattered by a script
    // (seeded) over the dunes, never on the road (at least 7 m from its centre
    // line) nor in the starting clearing: 14 inside the loop, 30 outside it
    struct Prop {
      const char *model;
      float x, z, yaw, scale;
    };
    const char *cactusA = "../assets/desert/cactus_a.obj";
    const char *cactusB = "../assets/desert/cactus_b.obj";
    const char *rockA = "../assets/desert/rock_a.obj";
    const char *rockB = "../assets/desert/rock_b.obj";
    const Prop propList[] = {
        {rockA, 7.1f, -18.7f, 0.3f, 1.3f},
        {cactusA, -13.3f, 8.6f, 6.3f, 0.9f},
        {cactusB, 1.8f, 18.4f, 5.6f, 1.8f},
        {cactusA, -7.5f, -15.2f, 1.7f, 1.4f},
        {rockA, 19.1f, 8.2f, 1.4f, 2.2f},
        {cactusA, 14.6f, -15.1f, 0.0f, 1.5f},
        {rockB, -6.4f, 12.5f, 0.9f, 1.4f},
        {cactusB, -17.1f, -0.7f, 5.9f, 0.9f},
        {cactusB, 7.2f, -10.6f, 2.5f, 1.8f},
        {cactusB, -20.0f, -7.4f, 4.2f, 1.6f},
        {rockB, 16.9f, -6.6f, 5.1f, 2.0f},
        {cactusB, -8.9f, 19.7f, 2.6f, 0.9f},
        {rockB, 8.4f, 20.2f, 2.5f, 1.1f},
        {rockA, 13.0f, 15.6f, 3.1f, 1.3f},
        {rockB, 8.7f, -64.6f, 2.8f, 1.3f},
        {cactusA, -72.6f, -81.5f, 5.8f, 1.2f},
        {rockA, 63.9f, -3.2f, 5.4f, 1.9f},
        {cactusB, -26.7f, 48.2f, 4.6f, 1.4f},
        {cactusA, 32.4f, -81.1f, 1.3f, 1.0f},
        {cactusA, -48.7f, -74.0f, 5.6f, 1.4f},
        {cactusB, 46.8f, 72.3f, 2.1f, 1.5f},
        {rockB, 53.1f, -44.2f, 4.6f, 1.7f},
        {cactusA, -0.9f, -73.7f, 3.0f, 1.4f},
        {rockA, 54.6f, -78.7f, 3.8f, 1.6f},
        {rockB, -50.7f, 80.4f, 1.8f, 2.3f},
        {cactusA, -41.5f, 59.2f, 1.6f, 1.5f},
        {rockA, -71.2f, -50.4f, 3.4f, 1.7f},
        {cactusA, -1.9f, 69.3f, 1.6f, 1.6f},
        {cactusA, -50.3f, -28.9f, 2.3f, 1.7f},
        {cactusA, -53.2f, -82.0f, 2.6f, 1.1f},
        {rockB, 16.7f, 78.9f, 3.8f, 1.3f},
        {rockB, 69.1f, -65.3f, 4.9f, 1.5f},
        {rockA, 42.6f, 53.0f, 6.0f, 1.5f},
        {cactusB, -20.6f, 62.3f, 2.3f, 1.2f},
        {cactusB, -72.2f, -36.5f, 3.4f, 1.0f},
        {cactusA, -9.5f, -77.9f, 0.8f, 1.7f},
        {cactusA, -20.8f, 73.9f, 5.0f, 1.3f},
        {rockB, -78.9f, -70.9f, 1.6f, 1.9f},
        {cactusA, 64.4f, 73.8f, 1.2f, 1.4f},
        {cactusB, 35.0f, -50.9f, 2.4f, 1.3f},
        {cactusB, 29.9f, 47.3f, 0.4f, 1.3f},
        {rockA, -30.1f, 63.4f, 0.9f, 2.3f},
        {cactusB, -60.6f, 2.4f, 5.1f, 1.2f},
        {rockB, -79.8f, 81.9f, 1.2f, 1.9f},
    };
    for (const Prop &p : propList) {
      auto o = make_shared<GameObject>(loadModel(p.model));
      // sink the base a little so nothing floats on the slopes
      o->setPosition(p.x, groundAt(p.x, p.z) - 0.05f, p.z);
      o->setYaw(p.yaw);
      o->setScale(p.scale);
      add(o);
    }

    // The creature, standing in the distance and facing the camera (disabled)
    // auto creature = make_shared<DynamicGameObject>(
    //     loadModel("../assets/creature/creature.obj"));
    // creature->addPart(loadModel("../assets/creature/creature_eyes.obj"),
    //                   2); // the eyes glow
    // creature->setPosition(1.5f, GROUND_Y + 0.91f, -13.0f); // see terrain_height()
    // creature->setYaw(0.25f);
    // creature->setBreathAmp(2.0f); // the creature breathes
    // addDynamic(creature);

    // The RV, facing +z: its door (+x side) is towards the start
    createRV(sound, 0.0f, 0.0f, 0.0f);
    createWalker(3.0f, 4.0f);

    // A satellite next to the start, within reach (see Interactable)
    auto satellite = make_shared<Satellite>(
        loadModel("../assets/antenna/antenna_dish.obj"),
        loadModel("../assets/antenna/antenna_base.obj"),
        vec3(4.5f, groundAt(4.5f, 2.0f), 2.0f));
    add(satellite);
    add(satellite->getMount());
    interactables.push_back(satellite.get());

    // An NPC a few steps ahead of the start, facing it: talk to it with the
    // Use key. Same model as the player, standing on its feet.
    VoiceSettings voice;
    voice.pitch = 62; // a bit higher than the default
    // Pingu dances; while he talks to the player he stands still, breathing
    // calmly (the same model in its idle pose): both are loaded
    auto dancing = make_shared<AnimatedModel>(
        "../assets/ping/PenguinoAnimado.fbx", true, PENGUIN_ANIMATION);
    auto standing = make_shared<AnimatedModel>(
        "../assets/ping/PenguinoAnimado.fbx", true, PENGUIN_ANIMATION);
    standing->setIdle(true);
    auto guide = make_shared<Pingu>(
        dancing, standing,
        "Pingu", std::vector<std::string>{
            "¡Hola, viajero! Soy Pingu y vigilo esta antena en mitad del desierto.",
            "Acércate al satélite y úsalo: puedes girarlo en azimut y en cénit para apuntar a cualquier punto del cielo.",
            "Dicen que de noche este desierto cambia por completo. Yo, por si acaso, me quedo aquí.",
        },
        sound, speech, voice);
    guide->setPosition(3.0f, groundAt(3.0f, 0.5f), 0.5f);
    guide->faceTowards(vec3(3.0f, 0.0f, 4.0f));
    guide->setGravity(25.0f);
    addDynamic(guide);
    interactables.push_back(guide.get());

    createCreature(sound, speech, 18.0f, 24.0f);
    // The other NPCs are its prey if they come near
    Npc *pingu = guide.get();
    creature->setPreyQuery([pingu]() { return std::vector<Npc *>{pingu}; });

    // A pool of water (for now a blue square, flat on a flat bit of sand): the mosquito lays its
    // eggs in it
    pool = vec3(-44.0f, 0.0f, 67.0f);
    pool.y = std::max(std::max(groundAt(pool.x - 2.0f, pool.z - 2.0f), groundAt(pool.x + 2.0f, pool.z - 2.0f)),
                      std::max(groundAt(pool.x - 2.0f, pool.z + 2.0f), groundAt(pool.x + 2.0f, pool.z + 2.0f)));
    auto puddle = make_shared<GameObject>(loadModel("../assets/water/puddle.obj"));
    puddle->setPosition(pool.x, pool.y + 0.03f, pool.z);
    puddle->setCollidable(false); // (before add: a static is registered when added)
    add(puddle);

    // A giant mosquito: it roams its corner of the desert looking for water to lay its eggs in,
    // and at dawn and at dusk, when the player comes near, it circles him and dives to bite (see
    // Mosquito). Running it over with the RV kills it. Its eggs hatch into more (layEgg).
    vec3 nest(-35.0f, 0.0f, 40.0f);
    nest.y = groundAt(nest.x, nest.z);
    // (it starts with blood in its stomach: it can lay its eggs)
    addMosquito(nest + vec3(0.0f, 5.0f, 0.0f), nest, 1.0f, false)->setBlood(1.0f);

    // A sign to read (no voice: the text types itself out), past the
    // satellite, turned towards the start
    auto sign = make_shared<Readable>(
        loadModel("../assets/sign/sign.obj"), "Cartel",
        std::vector<std::string>{
            "AVISO: estación de seguimiento del desierto. Prohibido el paso a personal no autorizado.",
            "La antena se orienta con el azimut y el cénit. No la apuntéis nunca directamente al sol.",
            "Si de noche veis algo moverse entre las dunas, no os acerquéis. Volved a la carretera.",
        },
        1.3f); // the board's height
    sign->setPosition(7.5f, groundAt(7.5f, -1.0f), -1.0f);
    sign->setYaw(std::atan2(3.0f - 7.5f, 4.0f + 1.0f)); // face (3, 4)
    add(sign);
    interactables.push_back(sign.get());
  }
};

// Shaders and assets are loaded with paths relative to src/. The binary is
// built into test/, next to src/, so move there whatever the launch directory.
bool enterSourceDir() {
  char exe[PATH_MAX];
  ssize_t length = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
  if (length <= 0)
    return false;
  exe[length] = '\0';
  std::string dir(exe);
  dir = dir.substr(0, dir.find_last_of('/')) + "/../src";
  return chdir(dir.c_str()) == 0;
}

int main(int argc, char **argv) {
  FloorMode floorMode = FloorMode::HeightField; // pass --ray for DownwardRay
  float startHour = -1.0f;   // --time H: hour a map starts at (default: its own)
  float dayDuration = -1.0f; // --day-duration S: seconds per day (0 = stopped)
  std::string startMap;      // --map N|NAME: the map to start in (number in the list from 0, or name)
  bool profile = false;      // --profile: print where the time of each frame goes
  for (int i = 1; i < argc; i++)
    if (std::string(argv[i]) == "--windowed")
      FULLSCREEN = false;
    else if (std::string(argv[i]) == "--ray")
      floorMode = FloorMode::DownwardRay;
    else if (std::string(argv[i]) == "--time" && i + 1 < argc)
      startHour = (float)atof(argv[++i]);
    else if (std::string(argv[i]) == "--day-duration" && i + 1 < argc)
      dayDuration = (float)atof(argv[++i]);
    else if (std::string(argv[i]) == "--map" && i + 1 < argc)
      startMap = argv[++i];
    else if (std::string(argv[i]) == "--profile")
      profile = true;
  if (!enterSourceDir())
    fprintf(stderr, "Could not find src/, using the current directory\n");

  // Creation of window and it's context
  GLFWwindow *window;
  if (!(window = initializeGLFW(WINDOWNAME)))
    return -1;

  // Enable depth test
  glEnable(GL_DEPTH_TEST);

  // Initialization of shaders
  Shader shader(vertexShaderFile, fragmentShaderFile);

  // Creation of camera
  Camera camera(window, &shader);
  camera.reposition(0.0, 0.0, 3.0);
  // The player's options from previous sessions (FOV, sensitivity...)
  Settings settings;
  settings.load();
  CameraMenu::applySettings(settings, camera);

  // Which key does what, read by everything that handles input; the player
  // may have rebound some (ControlsMenu)
  Controls controls;
  controls.readFrom(settings);
  Controller controller(window, &camera, controls);

  // Draws the particles (dust...) of the current map
  ParticleRenderer particles;

  // Audio: the output, and the text-to-speech the NPCs talk with
  SoundEngine sound;
  AudioMenu::applySettings(settings, sound); // the saved volumes
  MusicPlayer music(sound); // the current map's background music
  // and its ambient sound (wind...), apart: it is one of the game's sounds, not music
  MusicPlayer ambience(sound, SoundEngine::Channel::Game);
  EspeakSynthesizer speech;

  // The single light (the sun or the moon, set by each map)
  Light light(1.0f, 1.0f, 1.0f, &shader);
  shader.setInt("unlit", 0);

  // 2D interface over the scene
  UIManager ui(window);
  // Objects the player can use (key E), each with its own panel
  InteractionSystem interaction(window, &ui, controls);
  // Debug: select objects, see their data, move them and change their values
  // (keys 1, 2 and 0, see DebugSelector)
  DebugSelector selector(window, ui, controls);
  // The player's death: the screen goes red (the camera falls: see the main loop)
  DeathOverlay deathOverlay;
  ui.addOverlay(&deathOverlay);
  // After it, the end credits (read from a file) roll over the black screen
  CreditsOverlay credits;
  credits.load("../assets/credits/credits.txt");
  ui.addOverlay(&credits);
  // Paralysed by Bob's ray: the screen goes yellow for a while (the same kind of tint)
  DeathOverlay paralysisOverlay;
  paralysisOverlay.setColor(vec3(1.0f, 0.9f, 0.15f));
  ui.addOverlay(&paralysisOverlay);
  // Held by Bob: which key to hammer to get free, and how near he is
  StruggleOverlay struggleOverlay([&controls]() { return controls.keyName(Action::LeaveVehicle); });
  ui.addOverlay(&struggleOverlay);
  // Bob is near: the picture gets grainy (see GameStage::alienPresence)
  FilmGrain grain;
  // Abducted: a scream (not too loud) while he rises, and a rip as he goes into the ship
  const float SCREAM_VOLUME = 1.2f, RIP_VOLUME = 1.0f;
  auto screamClip = std::make_shared<AudioClip>(), ripClip = std::make_shared<AudioClip>();
  if (!screamClip->loadWavFile("../assets/bob/bob_scream.wav"))
    screamClip.reset();
  if (!ripClip->loadWavFile("../assets/bob/bob_rip.wav"))
    ripClip.reset();
  std::unique_ptr<Sound> scream, rip;
  double deathTime = -1.0;       // seconds since he died (-1: alive)
  vec3 abductFrom = vec3(0.0f);  // (abducted) where the camera was when Bob took him
  float deathStartPitch = 0.0f;  // where the camera was looking when it happened
  float deathEyeHeight = 1.6f;   // ...how high it was above his feet
  float deathAngle = 0.0f, deathSpeed = 0.0f; // the fall: how far over it is and how fast it tips (radians)

  // The maps, in the order the debug selector (key Z) lists them
  struct Map {
    std::string name;
    std::function<std::unique_ptr<GameStage>()> create; // nullptr on error
  };
  const std::vector<Map> maps = {
      {"Desierto de dia",
       [floorMode, &sound, &speech]() {
         return std::unique_ptr<GameStage>(
             new TestStage(floorMode, sound, speech));
       }},
      {"Bosque",
       [floorMode, &sound]() -> std::unique_ptr<GameStage> {
         std::unique_ptr<ForestStage> forest(new ForestStage(floorMode, sound));
         if (!forest->isValid())
           return nullptr;
         return std::unique_ptr<GameStage>(forest.release());
       }},
      {"Ruta 66",
       [floorMode, &sound, &speech]() -> std::unique_ptr<GameStage> {
         std::unique_ptr<Route66Stage> route(new Route66Stage(floorMode, sound, speech));
         if (!route->isValid())
           return nullptr;
         return std::unique_ptr<GameStage>(route.release());
       }},
      {"Desierto de noche",
       [floorMode]() -> std::unique_ptr<GameStage> {
         return SceneStage::load("../assets/scenes/desert.scene",
                                 "../assets", floorMode);
       }},
      {"Bosque de pinos",
       [floorMode, &sound]() {
         return std::unique_ptr<GameStage>(new PineForestStage(floorMode, sound));
       }},
  };
  std::unique_ptr<GameStage> stage;
  int currentMap = -1;
  int requestedMap = 0; // switched to at a safe point of the main loop
  for (size_t i = 0; i < maps.size() && !startMap.empty(); i++)
    if (startMap == maps[i].name || startMap == std::to_string(i))
      requestedMap = (int)i;
  bool resetRequested = false; // start the current map again, there too

  // Hands the map's light and sky colours to the shader (they change with
  // its time of day, so this is done every frame)
  // Hands the spot lights that are on (the vehicle's headlights) to the shader
  // every frame; the shader has room for MAX_SPOTS of them
  std::vector<SpotLight> lights; // the ones on this frame (also for the particles)
  auto applySpotLights = [&]() {
    const int MAX_SPOTS = 8; // as in shader.frag
    lights.clear();
    stage->getSpotLights(lights);
    if ((int)lights.size() > MAX_SPOTS)
      lights.resize(MAX_SPOTS);
    shader.use();
    shader.setInt("spotCount", (int)lights.size());
    for (size_t i = 0; i < lights.size(); i++) {
      const SpotLight &l = lights[i];
      std::string n = "[" + std::to_string(i) + "]";
      shader.setVector3(("spotPosition" + n).c_str(), l.position.x, l.position.y, l.position.z);
      shader.setVector3(("spotDirection" + n).c_str(), l.direction.x, l.direction.y, l.direction.z);
      shader.setVector3(("spotColor" + n).c_str(), l.color.r, l.color.g, l.color.b);
      shader.setVector3(("spotParams" + n).c_str(), l.innerCos, l.outerCos, l.range);
    }
  };

  // The texture unit of a canopy's mask (Environment::canopyMask): far from the models' (0, 1...)
  const int CANOPY_TEXTURE_UNIT = 7;
  auto applyEnvironment = [&]() {
    const Environment &env = stage->getEnvironment();
    vec3 lightPosition = env.lightDir * 100.0f; // only its direction is used
    shader.use();
    light.setColor(env.lightColor.r, env.lightColor.g, env.lightColor.b);
    light.moveTo(lightPosition.x, lightPosition.y, lightPosition.z);
    shader.setVector3("moonDir", env.lightDir.x, env.lightDir.y, env.lightDir.z);
    shader.setVector3("fogColor", env.horizon.r, env.horizon.g, env.horizon.b);
    shader.setVector3("skyZenith", env.skyZenith.r, env.skyZenith.g, env.skyZenith.b);
    shader.setVector3("sunDir", env.sunDir.x, env.sunDir.y, env.sunDir.z);
    shader.setFloat("starAlpha", env.starAlpha);
    shader.setInt("skyDunes", env.skyDunes ? 1 : 0);
    // the canopy's mask (a forest) on a texture unit of its own, past the models' ones
    shader.setInt("canopyOn", env.canopyMask ? 1 : 0);
    shader.setInt("canopyMask", CANOPY_TEXTURE_UNIT);
    glActiveTexture(GL_TEXTURE0 + CANOPY_TEXTURE_UNIT);
    glBindTexture(GL_TEXTURE_2D, env.canopyMask);
    glActiveTexture(GL_TEXTURE0);
    if (env.canopyMask) {
      shader.setVector2("canopyMin", env.canopyMin.x, env.canopyMin.y);
      shader.setFloat("canopySize", env.canopySize);
      shader.setVector3("canopyHeights", env.canopyHeights.x, env.canopyHeights.y, env.canopyHeights.z);
      shader.setFloat("canopyStrength", env.canopyStrength);
    }
    shader.setFloat("forestHorizon", env.forestHorizon);
  };

  // Replaces the current map: nothing may still point into the old one (its
  // panels, the interaction targets, the controller's character)
  bool mapLoaded = false; // switchMap ran this frame (see the main loop)
  auto switchMap = [&](int index) {
    std::unique_ptr<GameStage> next = maps[index].create();
    if (!next) {
      fprintf(stderr, "Could not load the map '%s'\n", maps[index].name.c_str());
      return;
    }
    ui.closeAll();
    interaction.clear();
    selector.clear();
    stage = std::move(next);
    currentMap = index;
    // Each map brings its own music (or none, which silences the previous)
    music.play(stage->getMusic(), stage->isMusicLooping(),
               stage->getMusicVolume());
    ambience.play(stage->getAmbience(), true, stage->getAmbienceVolume());
    for (Interactable *object : stage->getInteractables())
      interaction.add(object);
    camera.setFarPlane(stage->getFarPlane());
    controller.attach(stage->getPlayer().get(), stage->getCameraDistance(),
                      stage->getCameraHeight(), stage->getCameraYaw());

    if (startHour >= 0.0f)
      stage->setTimeOfDay(startHour);
    if (dayDuration >= 0.0f)
      stage->setDayDuration(dayDuration);
    deathTime = -1.0; // (a new map: the player is alive)
    credits.stop();
    scream.reset();
    rip.reset();
    mapLoaded = true;
  };

  // Keys with no panel open: Esc shows the pause menu (whose "Salir" / Quit
  // key ends the game), the Maps key (Z) the debug map selector
  auto quit = [window]() { glfwSetWindowShouldClose(window, true); };
  MenuContext menus = {ui, camera, controls, settings, sound, quit};
  ui.bindKey(GLFW_KEY_ESCAPE, [&]() { ui.open(new PauseMenu(menus)); });
  std::vector<std::string> mapNames;
  for (const Map &map : maps)
    mapNames.push_back(map.name);
  ui.bindKey([&controls]() { return controls.key(Action::Maps); }, [&]() {
    ui.open(new MapSelector(mapNames, currentMap, controls.key(Action::Maps),
                            [&](int index) { requestedMap = index; }));
  });

  // Debug select key (1): turns the object selection mode on and off
  ui.bindKey([&controls]() { return controls.key(Action::DebugSelect); },
             [&]() { selector.toggleSelect(); });
  // Debug place key (2): moves the selected object where the camera points
  ui.bindKey([&controls]() { return controls.key(Action::DebugPlace); },
             [&]() { selector.togglePlace(); });
  // Debug properties key (0): see the values of what the crosshair points at,
  // and change them with a click
  ui.bindKey([&controls]() { return controls.key(Action::DebugInspect); },
             [&]() { selector.toggleInspect(); });

  // Leave-vehicle key (with no panel open): the map puts the player back on
  // foot, if it was driving
  // (not while the debug placement mode turns an object: Shift snaps it
  // then)
  ui.bindKey([&controls]() { return controls.key(Action::LeaveVehicle); },
             [&]() {
               if (!selector.capturesMouse())
                 stage->leaveVehicle();
             });

  // Headlights key: the map turns its vehicle's lights on or off
  ui.bindKey([&controls]() { return controls.key(Action::Headlights); },
             [&]() { stage->toggleHeadlights(); });

  // Engine key: switches the vehicle's engine on or off
  ui.bindKey([&controls]() { return controls.key(Action::Engine); },
             [&]() { stage->toggleEngine(); });

  // Handbrake key: pulls or releases the vehicle's handbrake
  ui.bindKey([&controls]() { return controls.key(Action::Handbrake); },
             [&]() { stage->toggleHandbrake(); });

  // Vehicle camera key: inside the vehicle or from behind
  ui.bindKey([&controls]() { return controls.key(Action::VehicleCamera); },
             [&]() { stage->toggleVehicleCamera(); });

  // Ship-legs key: flying Bob's ship, its legs go in or out
  ui.bindKey([&controls]() { return controls.key(Action::ShipLegs); },
             [&]() { stage->toggleShipLegs(); });

  // The command console (key T): the commands it knows, and what has been
  // typed in it (kept while the game runs)
  Commands commands;
  commands.add("reset", "empieza el mapa de nuevo, como al arrancar el juego",
               [&](const std::vector<std::string> &) {
                 resetRequested = true; // not from inside the UI's update
                 return std::string("Reiniciando el mapa...");
               });
  // "day" and "night": the clock of the map jumps to the middle of the day or
  // of the night and goes on from there. In the day map (TestStage) the sun
  // rises at 6:00 and sets at 18:00; it is full day from about 7:30 to 16:30
  // and full night (dark, all the stars) from about 19:00 to 5:00.
  const float DAY_HOUR = 12.0f, NIGHT_HOUR = 0.0f;
  auto setHour = [&](float hour) {
    stage->setTimeOfDay(hour);
    return textFormat("Hora: %02d:00", (int)hour);
  };
  commands.add("day", "pone el mediodía (12:00)",
               [&](const std::vector<std::string> &) { return setHour(DAY_HOUR); });
  commands.add("night", "pone la medianoche (0:00)",
               [&](const std::vector<std::string> &) { return setHour(NIGHT_HOUR); });
  std::vector<std::string> commandHistory;
  auto openConsole = [&](const std::string &text) {
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    const float MARGIN = 16.0f;
    UIPanel *console = ui.open(new CommandConsole(commands, commandHistory,
                                                  width - 2 * MARGIN, text));
    console->moveTo(MARGIN, height - console->preferredHeight() - MARGIN);
  };
  ui.bindKey([&controls]() { return controls.key(Action::Console); },
             [&]() { openConsole(""); });
  // Typing '/' (whatever key it is on the layout) opens it with the '/' of a
  // command already written
  ui.bindChar('/', [&]() { openConsole("/"); });

  // Main loop
  double lastTime = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    // Clear the screen. It can cause flickering, so it's there nonetheless.
    double now = glfwGetTime();
    double dt = now - lastTime;
    lastTime = now;

    // "reset" (console): a new copy of the current map, as if the game had
    // just started (also without the debug modes)
    if (resetRequested) {
      resetRequested = false;
      selector.turnOff();
      switchMap(currentMap);
    }
    // A map change asked for (at start, or from the selector) happens here,
    // outside of any UI callback
    if (requestedMap >= 0) {
      if (requestedMap != currentMap)
        switchMap(requestedMap);
      requestedMap = -1;
      if (!stage)
        break; // not even the first map could be loaded
    }
    // Loading a map takes a while: that time must not reach the next frame as
    // one huge step of the physics
    if (mapLoaded) {
      mapLoaded = false;
      lastTime = glfwGetTime();
      dt = 0.0;
    }

    camera.resize();
    // The interface first: while any panel (menu or object) is open, the
    // player's controls are paused and the cursor is free
    interaction.update(stage->getPlayer()->getPosition(),
                       stage->interactionsEnabled());
    ui.update();
    // The map handed the controls to another character (got in or out of a
    // vehicle): the controller and the camera follow it
    if (stage->takePlayerChange())
      controller.attach(stage->getPlayer().get(), stage->getCameraDistance(),
                        stage->getCameraHeight(), stage->getCameraYaw());
    controller.setEnabled(!ui.hasPanels() && !stage->isPlayerDead() && !stage->playerImmobilized());
    paralysisOverlay.setAmount(0.35f * stage->playerParalysis() * (0.8f + 0.2f * (float)std::sin(now * 9.0)));
    struggleOverlay.setProgress(stage->struggleProgress());
    // In the debug placement mode, the right button turns the selected object
    // with the mouse instead of the camera
    controller.setLookEnabled(!selector.capturesMouse());
    controller.update();
    stage->setViewer(camera.getPosition(), camera.getViewProjection()); // (as it was the last frame)
    double tUpdate = glfwGetTime();
    stage->update(dt);
    double tPhysics = glfwGetTime() - tUpdate;
    // a map may fade its sounds (with the time of day)
    music.setVolume(stage->getMusicVolume());
    ambience.setVolume(stage->getAmbienceVolume());
    stage->getPlayer()->followCamera();
    // The player is dead: the camera falls over backwards like an inverted pendulum (the eyes at the
    // top of a rod standing on his feet, tipping over: slow at first, then faster and faster) and
    // ends lying on the ground looking up at the sky, with a little roll; the screen goes red
    if (stage->isPlayerDead() && stage->isPlayerAbducted()) {
      // Abducted (Bob caught him): he floats up in the ship's beam towards its hatch, turning
      // slowly and looking up into the light; the screen goes white, then black
      const float RISE_TIME = 6.0f, WHITE_FROM = 2.5f, BLACK_FROM = 5.5f, BLACK_TIME = 1.5f, CREDITS_PAUSE = 1.5f;
      if (deathTime < 0.0) {
        deathTime = 0.0;
        abductFrom = camera.getPosition();
        deathStartPitch = camera.getPitch();
        if (screamClip && (scream = sound.play(screamClip)))
          scream->setVolume(SCREAM_VOLUME);
      }
      bool arrives = deathTime < RISE_TIME && deathTime + dt >= RISE_TIME;
      deathTime += dt;
      if (arrives) { // (in the ship: the scream and Bob's hiss are cut short)
        scream.reset();
        stage->endAlienHiss();
        if (ripClip && (rip = sound.play(ripClip)))
          rip->setVolume(RIP_VOLUME);
      }
      float t = glm::min((float)deathTime / RISE_TIME, 1.0f);
      vec3 into = stage->getAbductPoint() - vec3(0.0f, 0.4f, 0.0f);
      vec3 eye = glm::mix(abductFrom, into, t * t * (3.0f - 2.0f * t));
      camera.reposition(eye.x, eye.y, eye.z);
      camera.setAngles(camera.getYaw() + 0.5f * (float)dt, glm::mix(deathStartPitch, -1.3f, glm::min(t * 2.0f, 1.0f)));
      camera.setCarrier(mat3(1.0f));
      float time = (float)deathTime;
      if (time < BLACK_FROM) {
        deathOverlay.setColor(vec3(0.85f, 0.95f, 1.0f));
        deathOverlay.setAmount(0.85f * glm::clamp((time - WHITE_FROM) / (BLACK_FROM - WHITE_FROM), 0.0f, 1.0f));
      } else {
        deathOverlay.setColor(vec3(0.0f));
        deathOverlay.setAmount(glm::min((time - BLACK_FROM) / BLACK_TIME, 1.0f));
      }
      if (time > BLACK_FROM + BLACK_TIME + CREDITS_PAUSE && !credits.isRunning())
        credits.start();
    } else if (stage->isPlayerDead()) {
      // After the fall, the red fades slowly to black and the credits start rolling
      const float FADE_FROM = 3.5f, FADE_TIME = 6.0f, CREDITS_PAUSE = 1.5f;
      float fade = glm::clamp(((float)deathTime - FADE_FROM) / FADE_TIME, 0.0f, 1.0f);
      deathOverlay.setColor(glm::mix(vec3(0.7f, 0.0f, 0.02f), vec3(0.0f), fade));
      const float G = 9.81f, FALL_BOUNCE = 0.3f, LOOK_UP = -1.5f, ROLL = 0.2f, TINT = 0.65f;
      const float START_ANGLE = 0.04f, START_SPEED = 0.3f; // (the blow that starts it)
      const float HALF_TURN = 1.5707963f;
      vec3 feet = stage->getPlayer()->getPosition();
      if (deathTime < 0.0) {
        deathTime = 0.0;
        deathStartPitch = camera.getPitch();
        deathEyeHeight = glm::max(camera.getPosition().y - feet.y, 0.5f);
        deathAngle = START_ANGLE;
        deathSpeed = START_SPEED;
      }
      deathTime += dt;
      // theta'' = (3 g / 2 L) sin(theta): a rod of length L (the height of the eyes) falling over
      float L = deathEyeHeight;
      deathSpeed += 1.5f * G / L * std::sin(deathAngle) * (float)dt;
      deathAngle += deathSpeed * (float)dt;
      if (deathAngle >= HALF_TURN) { // it hits the ground: a small bounce, and it settles
        deathAngle = HALF_TURN;
        deathSpeed = deathSpeed > 0.0f ? -deathSpeed * FALL_BOUNCE : 0.0f;
      }
      float yaw = camera.getYaw();
      vec3 flatForward(std::sin(yaw), 0.0f, -std::cos(yaw));
      vec3 eye = feet - flatForward * (L * std::sin(deathAngle));
      eye.y = feet.y + glm::max(L * std::cos(deathAngle), 0.25f);
      camera.reposition(eye.x, eye.y, eye.z);
      camera.setAngles(yaw, glm::mix(deathStartPitch, LOOK_UP, deathAngle / HALF_TURN)); // (it ends looking straight up)
      // a little roll, most while it tips over (a turn about the way it looked)
      float roll = ROLL * std::sin(2.0f * deathAngle);
      camera.setCarrier(mat3(rotate(mat4(1.0f), roll, flatForward)));
      deathOverlay.setAmount(glm::mix(TINT * glm::min((float)deathTime / 0.4f, 1.0f), 1.0f, fade));
      if (fade >= 1.0f && deathTime > FADE_FROM + FADE_TIME + CREDITS_PAUSE && !credits.isRunning())
        credits.start();
    } else {
      deathOverlay.setAmount(0.0f);
    }
    credits.update((float)dt);
    // The camera is a body too: it can't sink into the floor (e.g. behind
    // the RV on a dune, or when the player walks up a slope)
    vec3 eye = camera.getPosition();
    if (stage->keepAboveFloor(eye, Camera::RADIUS))
      camera.reposition(eye.x, eye.y, eye.z);
    selector.countFrame(dt);
    selector.update(*stage, camera, !ui.hasPanels());
    // The player hears from the camera
    sound.setListener(camera.getPosition(), camera.getForward());
    const vec3 &horizon = stage->getEnvironment().horizon;
    applyEnvironment();
    applySpotLights();
    glClearColor(horizon.x, horizon.y, horizon.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw: the map's sky (if any) and the map, then the interface on top
    shader.setFloat("time", (float)now);
    RenderStats::clear();
    double tRender = glfwGetTime();
    static GLuint sampleQuery = 0;
    if (profile) { // how many samples the world's drawing leaves in the picture (its overdraw)
      if (!sampleQuery)
        glGenQueries(1, &sampleQuery);
      glBeginQuery(GL_SAMPLES_PASSED, sampleQuery);
    }
    stage->render(&shader, camera.getPosition(), now);
    if (profile)
      glEndQuery(GL_SAMPLES_PASSED);
    double tSubmit = glfwGetTime() - tRender; // the CPU's share: what it takes to give the GPU its work
    if (profile)
      glFinish(); // wait for the GPU, to time it
    double tGpu = glfwGetTime() - tRender - tSubmit;
    const Environment &env = stage->getEnvironment();
    particles.setLighting(env.lightColor, env.lightDir, lights);
    particles.draw(stage->getEmitters(), camera); // over the world
    selector.draw(camera); // the selected object's outline, if any
    grain.draw(stage->alienPresence(), (float)now);
    ui.draw(); // last, over everything

    // Swap buffers
    double tSwap = glfwGetTime();
    glfwSwapBuffers(window);
    glfwPollEvents();
    if (profile) {
      // The average of each second: frame time, then where it went (physics, submitting the
      // draw calls, the GPU finishing, waiting at the swap: vsync) and what was drawn
      static double sum[5] = {0, 0, 0, 0, 0}, since = glfwGetTime();
      static unsigned long frames = 0, draws = 0, tris = 0, uniforms = 0, objects = 0;
      static double samples = 0.0;
      GLuint passed = 0;
      glGetQueryObjectuiv(sampleQuery, GL_QUERY_RESULT, &passed);
      samples += passed;
      sum[0] += glfwGetTime() - now;
      sum[1] += tPhysics;
      sum[2] += tSubmit;
      sum[3] += tGpu;
      sum[4] += glfwGetTime() - tSwap;
      frames++;
      draws += RenderStats::draws();
      tris += RenderStats::triangles();
      uniforms += RenderStats::uniformCalls();
      objects += RenderStats::objects();
      if (glfwGetTime() - since >= 1.0 && frames > 0) {
        fprintf(stderr,
                "PROFILE %.1f fps | frame %.1f ms = physics %.1f + submit %.1f + gpu %.1f + swap %.1f | "
                "%lu objects, %lu draw calls, %.2fM triangles, %lu uniform calls per frame\n",
                frames / (glfwGetTime() - since), 1000 * sum[0] / frames, 1000 * sum[1] / frames,
                1000 * sum[2] / frames, 1000 * sum[3] / frames, 1000 * sum[4] / frames,
                objects / frames, draws / frames, tris / frames / 1e6, uniforms / frames);
        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        fprintf(stderr, "        %.1f samples per pixel of the world (overdraw, counting each MSAA sample)\n",
                samples / frames / ((double)fbw * fbh));
        samples = 0.0;
        for (double &v : sum) v = 0;
        frames = draws = tris = uniforms = objects = 0;
        since = glfwGetTime();
      }
    }
  }

  glfwTerminate();
  return 0;
}
