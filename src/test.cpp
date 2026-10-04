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
#include "EspeakSynthesizer.h"
#include "GameStage.h"
#include "GameObject.h"
#include "InteractionSystem.h"
#include "Light.h"
#include "MapSelector.h"
#include "PlayableCharacter.h"
#include "RV.h"
#include "Readable.h"
#include "Satellite.h"
#include "Model.h"
#include "Npc.h"
#include "PauseMenu.h"
#include "SceneStage.h"
#include "Shader.h"
#include "Stage.h"
#include "Skeleton.h"
#include "SoundEngine.h"
#include "UIManager.h"
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
const char *vertexShaderFile = "animatedshader.vert";

// Fragment shader
const char *fragmentShaderFile = "shader.frag";

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

// The desert: dunes with a road winding through them, cacti and rocks, a
// parked RV, a satellite the player can orient, and the player: a penguin
// on foot, seen in first person.
class TestStage : public GameStage {
private:
  static constexpr float GROUND_Y = -1.0f; // ground level of the clearing
  std::shared_ptr<RV> rv;

  // Ground height at (x, z), or GROUND_Y where there is no floor
  float groundAt(float x, float z) const {
    return GameStage::groundAt(x, z, GROUND_Y);
  }

protected:
  // The RV and the player stay on the dunes
  void apply(DynamicGameObject &object, double dt) override {
    collideWithFloor(object);
  }

public:
  // The NPCs speak through `sound` with voices made by `speech`
  TestStage(FloorMode mode, SoundEngine &sound, SpeechSynthesizer &speech)
      : GameStage(mode) {
    // Daylight: plain blue sky (the clear colour), distant geometry fades
    // into it; warm white sunlight
    environment.lightDir = normalize(vec3(-0.3f, 0.8f, -0.5f));
    environment.lightColor = vec3(0.85f, 0.83f, 0.78f);
    environment.horizon = vec3(0.45f, 0.68f, 0.92f);
    // First person: the camera at the eyes, 1.6 above the feet (the
    // penguin is 1.8 tall)
    cameraDistance = 0.0f;
    cameraHeight = 1.6f;

    // Desert scenery
    auto ground = make_shared<GameObject>(loadModel("../assets/desert/dunes.obj"));
    ground->setPosition(0.0f, GROUND_Y, 0.0f);
    add(ground);
    // The dunes are a regular grid of heights, so they are the height map
    setFloor(loadModel("../assets/desert/dunes.obj"), vec3(0.0f, GROUND_Y, 0.0f));
    auto road = make_shared<GameObject>(loadModel("../assets/desert/road.obj")); // winds through the dunes
    road->setPosition(0.0f, GROUND_Y, 0.0f);
    add(road);

    // model, x, z, terrain height at (x, z) (see terrain_height in
    // generate_assets.py), rotation around y, uniform scale
    struct Prop {
      const char *model;
      float x, z, height, yaw, scale;
    };
    const char *cactusA = "../assets/desert/cactus_a.obj";
    const char *cactusB = "../assets/desert/cactus_b.obj";
    const char *rockA = "../assets/desert/rock_a.obj";
    const char *rockB = "../assets/desert/rock_b.obj";
    const Prop propList[] = {
        {cactusA, -4.5f, -8, 0.18f, 0.5f, 1.0f},
        {cactusA, 8, -16, 1.45f, 2.0f, 1.3f},
        {cactusA, -16, -24, 4.61f, 4.0f, 1.6f},
        {cactusA, 18, -30, 1.17f, 1.0f, 1.2f},
        {cactusB, 5.5f, -10, 0.72f, 3.0f, 1.1f},
        {cactusB, -9, -14, 1.61f, 5.5f, 1.2f},
        {cactusB, 14, -22, 1.62f, 0.8f, 1.5f},
        {rockA, -4, -6, 0.03f, 0.3f, 1.0f},
        {rockA, 11, -9, 2.23f, 2.5f, 1.8f},
        {rockA, -20, -12, 1.90f, 4.5f, 2.5f},
        {rockB, 3.5f, -4.5f, 0.01f, 1.2f, 1.0f},
        {rockB, -8, -11, 1.03f, 3.3f, 1.4f},
        {rockB, 7, -18, 1.11f, 5.0f, 1.7f},
        {rockB, -3, -14, 0.73f, 0.1f, 1.1f},
    };
    for (const Prop &p : propList) {
      auto o = make_shared<GameObject>(loadModel(p.model));
      // sink the base a little so nothing floats on the slopes
      o->setPosition(p.x, GROUND_Y + p.height - 0.05f, p.z);
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

    // The RV (front toward +z, wheels on y = 0)
    rv = make_shared<RV>(loadModel("../assets/rv/rv.obj"));
    rv->setPosition(0.0f, GROUND_Y, 0.0f);
    rv->setMaxSpeed(20.0f);
    rv->setGravity(25.0f);
    addDynamic(rv);

    // The player: the penguin on foot, its feet on the floor. It is drawn
    // centred on its position (AnimatedModel fits it to 1.8 units around the
    // origin), which only shows if the camera is moved out of first person.
    auto walker = make_shared<Walker>(
        make_shared<AnimatedModel>("../assets/ping/PenguinoAnimado.fbx"));
    walker->setPosition(3.0f, groundAt(3.0f, 4.0f), 4.0f);
    walker->setGravity(25.0f);
    addDynamic(walker);
    player = walker;

    // A satellite next to the start, within reach (see Interactable)
    auto satellite = make_shared<Satellite>(loadModel("../assets/cube/cube.obj"),
                                            vec3(4.5f, groundAt(4.5f, 2.0f), 2.0f));
    add(satellite);
    add(satellite->getMount());
    interactables.push_back(satellite.get());

    // An NPC a few steps ahead of the start, facing it: talk to it with the
    // Use key. Same model as the player, standing on its feet.
    VoiceSettings voice;
    voice.pitch = 62; // a bit higher than the default
    auto guide = make_shared<Npc>(
        make_shared<AnimatedModel>("../assets/ping/PenguinoAnimado.fbx", true),
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
  for (int i = 1; i < argc; i++)
    if (std::string(argv[i]) == "--windowed")
      FULLSCREEN = false;
    else if (std::string(argv[i]) == "--ray")
      floorMode = FloorMode::DownwardRay;
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
  // Which key does what, read by everything that handles input
  Controls controls;
  Controller controller(window, &camera, controls);

  // Audio: the output, and the text-to-speech the NPCs talk with
  SoundEngine sound;
  EspeakSynthesizer speech;

  // The single light (the sun or the moon, set by each map)
  Light light(1.0f, 1.0f, 1.0f, &shader);
  shader.setInt("unlit", 0);

  // 2D interface over the scene
  UIManager ui(window);
  // Objects the player can use (key E), each with its own panel
  InteractionSystem interaction(window, &ui, controls);

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
      {"Desierto de noche",
       [floorMode]() -> std::unique_ptr<GameStage> {
         return SceneStage::load("../assets/scenes/desert.scene",
                                 "../assets", floorMode);
       }},
  };
  std::unique_ptr<GameStage> stage;
  int currentMap = -1;
  int requestedMap = 0; // switched to at a safe point of the main loop

  // Replaces the current map: nothing may still point into the old one (its
  // panels, the interaction targets, the controller's character)
  auto switchMap = [&](int index) {
    std::unique_ptr<GameStage> next = maps[index].create();
    if (!next) {
      fprintf(stderr, "Could not load the map '%s'\n", maps[index].name.c_str());
      return;
    }
    ui.closeAll();
    interaction.clear();
    stage = std::move(next);
    currentMap = index;
    for (Interactable *object : stage->getInteractables())
      interaction.add(object);
    controller.attach(stage->getPlayer().get(), stage->getCameraDistance(),
                      stage->getCameraHeight());

    const Environment &env = stage->getEnvironment();
    vec3 lightPosition = env.lightDir * 100.0f; // far: almost directional
    light.setColor(env.lightColor.r, env.lightColor.g, env.lightColor.b);
    light.moveTo(lightPosition.x, lightPosition.y, lightPosition.z);
    shader.setVector3("moonDir", env.lightDir.x, env.lightDir.y, env.lightDir.z);
    shader.setVector3("fogColor", env.horizon.r, env.horizon.g, env.horizon.b);
  };

  // Keys with no panel open: Esc shows the pause menu (whose "Salir" / Quit
  // key ends the game), the Maps key (Z) the debug map selector
  auto quit = [window]() { glfwSetWindowShouldClose(window, true); };
  MenuContext menus = {ui, camera, controls, quit};
  ui.bindKey(GLFW_KEY_ESCAPE, [&]() { ui.open(new PauseMenu(menus)); });
  std::vector<std::string> mapNames;
  for (const Map &map : maps)
    mapNames.push_back(map.name);
  int mapsKey = controls.key(Action::Maps);
  ui.bindKey(mapsKey, [&, mapsKey]() {
    ui.open(new MapSelector(mapNames, currentMap, mapsKey,
                            [&](int index) { requestedMap = index; }));
  });

  // Main loop
  double lastTime = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    // Clear the screen. It can cause flickering, so it's there nonetheless.
    double now = glfwGetTime();
    double dt = now - lastTime;
    lastTime = now;

    // A map change asked for (at start, or from the selector) happens here,
    // outside of any UI callback
    if (requestedMap >= 0) {
      if (requestedMap != currentMap)
        switchMap(requestedMap);
      requestedMap = -1;
      if (!stage)
        break; // not even the first map could be loaded
    }

    camera.resize();
    // The interface first: while any panel (menu or object) is open, the
    // player's controls are paused and the cursor is free
    interaction.update(stage->getPlayer()->getPosition());
    ui.update();
    controller.setEnabled(!ui.hasPanels());
    controller.update();
    stage->update(dt);
    stage->getPlayer()->followCamera();
    // The player hears from the camera
    sound.setListener(camera.getPosition(), camera.getForward());
    const vec3 &horizon = stage->getEnvironment().horizon;
    glClearColor(horizon.x, horizon.y, horizon.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw: the map's sky (if any) and the map, then the interface on top
    shader.setFloat("time", (float)now);
    stage->render(&shader, camera.getPosition(), now);
    ui.draw(); // last, over everything

    // Swap buffers
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}
