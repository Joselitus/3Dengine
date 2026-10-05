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
#include "DebugSelector.h"
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
#include "FollaCulos.h"
#include "Pingu.h"
#include "AudioMenu.h"
#include "CameraMenu.h"
#include "PauseMenu.h"
#include "SceneStage.h"
#include "Settings.h"
#include "Shader.h"
#include "Stage.h"
#include "Skeleton.h"
#include "MusicPlayer.h"
#include "ParticleRenderer.h"
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
// goes back to first person.
class TestStage : public GameStage {
private:
  // PenguinoAnimado.fbx holds two takes of the same dance; take 0 (".002") has
  // the right flipper detached from the body and the feet in the air, take 1
  // (".003") is the clean one.
  static constexpr unsigned int PENGUIN_ANIMATION = 1;
  static constexpr float GROUND_Y = -1.0f; // ground level of the clearing
  static constexpr float CAR_CAMERA_DISTANCE = 12.0f;
  static constexpr float CAR_CAMERA_HEIGHT = 3.5f;
  static constexpr float EYE_HEIGHT = 1.6f; // first person, above the feet
  static constexpr float DAY_DURATION = 360.0f; // real seconds per 24 h
  // What is within this many metres of the edge of the terrain is not drawn
  static constexpr float EDGE_CULL_MARGIN = 12.0f;
  static constexpr float START_HOUR = 12.0f;    // the game starts at midday
  // What is left of the light with no sun (it comes from straight above): very
  // little, so that at night it is hard to see anything without the headlights
  const vec3 NIGHT_LIGHT = vec3(0.022f, 0.025f, 0.04f);
  std::shared_ptr<RV> rv;
  std::shared_ptr<Walker> walker; // the penguin on foot
  std::shared_ptr<FollaCulos> creature; // the night creature: runs at the player
  bool inVehicle = false;         // the penguin is inside the RV

  // The penguin gets into the RV: it is hidden inside its body and goes
  // wherever the RV goes, and the RV gets the controls and the camera
  void enterRV() {
    if (inVehicle)
      return;
    inVehicle = true;
    walker->control(vec2(0.0f), 0.0f, 0.0f); // stops walking
    walker->setVelocity(vec3(0.0f));
    walker->setGravity(0.0f);       // it rides: nothing pulls it down
    walker->setCollidable(false);   // inside the RV's box
    walker->setVisible(false);
    vec3 seat = rv->seatPosition();
    walker->setPosition(seat.x, seat.y, seat.z);
    rv->setOccupied(true);
    setPlayer(rv, CAR_CAMERA_DISTANCE, CAR_CAMERA_HEIGHT, rv->headingYaw());
  }

  // Ground height at (x, z), or GROUND_Y where there is no floor
  float groundAt(float x, float z) const {
    return GameStage::groundAt(x, z, GROUND_Y);
  }

protected:
  // The daylight cycle: the sun crosses the sky (east at 6:00, highest at
  // noon, west at 18:00). The sky, the fog and the light follow it: blue by
  // day, orange at dusk and dawn, and at night dark with stars and only a
  // minimum of light left. The music fades out as the sun sets (until only
  // the wind remains) and comes back with the dawn.
  void onTimeChanged() override {
    // 0 below `a`, 1 above `b`, smooth between (also works if a > b)
    auto ramp = [](float a, float b, float x) {
      float t = clamp((x - a) / (b - a), 0.0f, 1.0f);
      return t * t * (3.0f - 2.0f * t);
    };
    float angle = (getTimeOfDay() - 6.0f) / 24.0f * 6.2831853f;
    vec3 sun = normalize(vec3(cos(angle), sin(angle), -0.3f));
    float h = sun.y; // how high the sun is (negative: below the horizon)

    float day = ramp(-0.1f, 0.3f, h);
    float dusk = ramp(-0.2f, 0.0f, h) * (1.0f - ramp(0.05f, 0.35f, h));
    vec3 horizon = mix(vec3(0.035f, 0.055f, 0.11f), vec3(0.45f, 0.68f, 0.92f), day);
    horizon = mix(horizon, vec3(0.95f, 0.52f, 0.30f), dusk * 0.85f);
    vec3 zenith = mix(vec3(0.003f, 0.007f, 0.028f), vec3(0.16f, 0.38f, 0.78f), day);
    zenith = mix(zenith, vec3(0.25f, 0.22f, 0.45f), dusk * 0.5f);
    environment.horizon = horizon;
    environment.skyZenith = zenith;
    environment.sunDir = sun;
    environment.starAlpha = 1.0f - ramp(-0.2f, 0.0f, h);

    // The sun lights the world while it is up; at night there is only the
    // minimum that is always there, from straight above
    vec3 sunColor = mix(vec3(1.0f, 0.55f, 0.25f), vec3(0.85f, 0.83f, 0.78f),
                        ramp(0.0f, 0.45f, h));
    float sunPower = ramp(-0.05f, 0.25f, h);
    environment.lightColor = sunColor * sunPower + NIGHT_LIGHT;
    // The light comes from the sun while it gives noticeable light; only the
    // minimum that is left (NIGHT_LIGHT) comes from above, so as the sun sets
    // its share of the light shrinks instead of the direction drifting early
    float sunShare = sunPower / (sunPower + length(NIGHT_LIGHT));
    environment.lightDir = normalize(mix(vec3(0.0f, 1.0f, 0.0f), sun, sunShare));
    setMusicVolume(ramp(-0.1f, 0.5f, h)); // gone shortly after sunset
  }

  // Everything dynamic stays on the dunes (the penguin inside the RV just
  // rides in it)
  void apply(DynamicGameObject &object, double dt) override {
    if (inVehicle && &object == walker.get()) {
      vec3 seat = rv->seatPosition();
      object.setPosition(seat.x, seat.y, seat.z);
      object.setVelocity(vec3(0.0f));
      return;
    }
    collideWithFloor(object, dt);
  }

public:
  // Interactions are for the penguin on foot, not while driving
  bool interactionsEnabled() const override { return !inVehicle; }

  // The player (the penguin or the RV) is always drawn, wherever it is
  bool edgeCullExempt(const GameObject &object) const override {
    return &object == rv.get() || &object == walker.get();
  }

  // F: the headlights, only while the penguin is driving
  void toggleHeadlights() override {
    if (inVehicle)
      rv->toggleHeadlights();
  }
  // C: inside the RV or behind it, only while the penguin is driving
  void toggleVehicleCamera() override {
    if (inVehicle)
      rv->toggleCameraView();
  }
  void getSpotLights(std::vector<SpotLight> &lights) const override {
    rv->getHeadlights(lights);
    rv->getDashboardLights(lights);
    if (creature)
      creature->getLight(lights); // (last: if the shader has no room, the creature's is the one left out)
  }

  // The penguin gets out at the RV's door, on foot and in first person
  void leaveVehicle() override {
    if (!inVehicle)
      return;
    inVehicle = false;
    rv->control(vec2(0.0f), 0.0f, 0.0f); // the RV stops being driven
    rv->setOccupied(false);
    vec3 door = rv->doorPosition(1.5f); // beside the door, clear of the body
    walker->setPosition(door.x, groundAt(door.x, door.z), door.z);
    walker->setVelocity(vec3(0.0f));
    walker->setGravity(25.0f);
    walker->setCollidable(true);
    // (attaching it hides its model); it looks away from the RV
    setPlayer(walker, 0.0f, EYE_HEIGHT, rv->doorYaw());
  }

  // The NPCs speak through `sound` with voices made by `speech`
  TestStage(FloorMode mode, SoundEngine &sound, SpeechSynthesizer &speech)
      : GameStage(mode) {
    // The desert's background music: an arid guitar and banjo loop
    loadMusic("../assets/music/desert.wav");
    // ...and the wind, which never stops (it is all that is left at night)
    loadAmbience("../assets/music/wind.wav");

    // A day lasts DAY_DURATION seconds; the sky and the light follow the
    // clock (see onTimeChanged)
    setSky(loadModel("../assets/sky/skydome_plain.obj"), 3);
    setDayDuration(DAY_DURATION);
    setTimeOfDay(START_HOUR);
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

    // The RV (front toward +z, wheels on y = 0)
    rv = make_shared<RV>(loadModel("../assets/rv/rv.obj"));
    rv->setPosition(0.0f, GROUND_Y, 0.0f);
    rv->setHeading(0.0f); // facing +z: its door (+x side) is towards the start
    // The wheels are separate models so they follow the suspension
    rv->setWheelModels(loadModel("../assets/rv/wheel_negx.obj"),
                       loadModel("../assets/rv/wheel_posx.obj"));
    // The windshield: intact, and the cracked one that replaces it after a crash
    rv->setWindshieldModels(loadModel("../assets/rv/windshield.obj"),
                            loadModel("../assets/rv/windshield_broken.obj"));
    // The cockpit: the dashboard, the ignition key and the gauges' needle
    rv->setCockpitModels(loadModel("../assets/rv/dashboard.obj"),
                         loadModel("../assets/rv/key.obj"),
                         loadModel("../assets/rv/needle.obj"),
                         loadModel("../assets/rv/dashboard_glow.obj"));
    rv->setHeadlightGlowModel(loadModel("../assets/rv/headlight_glow.obj"));
    rv->setMaxSpeed(20.0f);
    rv->setGravity(25.0f);
    addDynamic(rv);
    // The dust its wheels throw up on sand (the stage moves and removes it)
    for (const auto &emitter : rv->getDust())
      addEmitter(emitter);
    for (const auto &emitter : rv->getGrains())
      addEmitter(emitter);
    // Using its door gets the player in (see enterRV)
    rv->setEnterAction([this]() { enterRV(); });
    interactables.push_back(rv.get());

    // The player: a penguin on foot (a Walker), its feet on the floor, seen
    // in first person. It is drawn centred on its position (AnimatedModel
    // fits it to 1.8 units around the origin), which only shows if the camera
    // is moved out of first person.
    walker = make_shared<Walker>(
        make_shared<AnimatedModel>("../assets/ping/PenguinoAnimado.fbx", false,
                                   PENGUIN_ANIMATION));
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

    // The night creature: it runs straight at whoever the player controls (the penguin on
    // foot, or the RV when driving). The model is its own size, in metres.
    creature = make_shared<FollaCulos>(
        make_shared<AnimatedModel>("../assets/folla_culos/folla_culos_run.glb", true),
        sound, speech);
    creature->setPosition(18.0f, groundAt(18.0f, 24.0f), 24.0f);
    creature->setGravity(25.0f);
    creature->setTarget([this]() { return player->getPosition(); });
    // At night it comes for you; by day it keeps away (the sun is below the horizon)
    creature->setNightQuery([this]() { return environment.sunDir.y < 0.0f; });
    addDynamic(creature);

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

constexpr unsigned int TestStage::PENGUIN_ANIMATION;

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
  for (int i = 1; i < argc; i++)
    if (std::string(argv[i]) == "--windowed")
      FULLSCREEN = false;
    else if (std::string(argv[i]) == "--ray")
      floorMode = FloorMode::DownwardRay;
    else if (std::string(argv[i]) == "--time" && i + 1 < argc)
      startHour = (float)atof(argv[++i]);
    else if (std::string(argv[i]) == "--day-duration" && i + 1 < argc)
      dayDuration = (float)atof(argv[++i]);
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
  AudioMenu::applySettings(settings, sound); // the saved master volume
  MusicPlayer music(sound); // the current map's background music
  MusicPlayer ambience(sound); // and its ambient sound (wind...), apart
  EspeakSynthesizer speech;

  // The single light (the sun or the moon, set by each map)
  Light light(1.0f, 1.0f, 1.0f, &shader);
  shader.setInt("unlit", 0);

  // 2D interface over the scene
  UIManager ui(window);
  // Objects the player can use (key E), each with its own panel
  InteractionSystem interaction(window, &ui, controls);
  // Debug: select objects, see their data and move them (keys 1 and 2, see
  // DebugSelector)
  DebugSelector selector(window, ui, controls);

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
  };

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
    selector.clear();
    stage = std::move(next);
    currentMap = index;
    // Each map brings its own music (or none, which silences the previous)
    music.play(stage->getMusic(), stage->isMusicLooping(),
               stage->getMusicVolume());
    ambience.play(stage->getAmbience(), true, stage->getAmbienceVolume());
    for (Interactable *object : stage->getInteractables())
      interaction.add(object);
    controller.attach(stage->getPlayer().get(), stage->getCameraDistance(),
                      stage->getCameraHeight(), stage->getCameraYaw());

    if (startHour >= 0.0f)
      stage->setTimeOfDay(startHour);
    if (dayDuration >= 0.0f)
      stage->setDayDuration(dayDuration);
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

  // Vehicle camera key: inside the vehicle or from behind
  ui.bindKey([&controls]() { return controls.key(Action::VehicleCamera); },
             [&]() { stage->toggleVehicleCamera(); });

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
    interaction.update(stage->getPlayer()->getPosition(),
                       stage->interactionsEnabled());
    ui.update();
    // The map handed the controls to another character (got in or out of a
    // vehicle): the controller and the camera follow it
    if (stage->takePlayerChange())
      controller.attach(stage->getPlayer().get(), stage->getCameraDistance(),
                        stage->getCameraHeight(), stage->getCameraYaw());
    controller.setEnabled(!ui.hasPanels());
    // In the debug placement mode, the right button turns the selected object
    // with the mouse instead of the camera
    controller.setLookEnabled(!selector.capturesMouse());
    controller.update();
    stage->update(dt);
    // a map may fade its sounds (with the time of day)
    music.setVolume(stage->getMusicVolume());
    ambience.setVolume(stage->getAmbienceVolume());
    stage->getPlayer()->followCamera();
    // The camera is a body too: it can't sink into the floor (e.g. behind
    // the RV on a dune, or when the player walks up a slope)
    vec3 eye = camera.getPosition();
    if (stage->keepAboveFloor(eye, Camera::RADIUS))
      camera.reposition(eye.x, eye.y, eye.z);
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
    stage->render(&shader, camera.getPosition(), now);
    const Environment &env = stage->getEnvironment();
    particles.setLighting(env.lightColor, env.lightDir, lights);
    particles.draw(stage->getEmitters(), camera); // over the world
    selector.draw(camera); // the selected object's outline, if any
    ui.draw(); // last, over everything

    // Swap buffers
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}
