#include <GL/glew.h>
#include <cstdlib>
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
#include "ConnectMenu.h"
#include "MapList.h"
#include "NetClient.h"
#include "NetOverlay.h"
#include "NetRole.h"
#include "Paths.h"
#include "Commands.h"
#include "TextFormat.h"
#include "DebugSelector.h"
#include "CreditsOverlay.h"
#include "DeathOverlay.h"
#include "CrosshairOverlay.h"
#include "FilmGrain.h"
#include "StruggleOverlay.h"
#include "EspeakSynthesizer.h"
#include "GameStage.h"
#include "GameObject.h"
#include "InteractionSystem.h"
#include "Light.h"
#include "PlayableCharacter.h"
#include "RenderStats.h"
#include "Model.h"
#include "AudioMenu.h"
#include "CameraMenu.h"
#include "PauseMenu.h"
#include "Settings.h"
#include "Shader.h"
#include "Stage.h"
#include "MusicPlayer.h"
#include "ParticleRenderer.h"
#include "SoundEngine.h"
#include "UIManager.h"
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

#if defined(GLFW_PLATFORM) && !defined(__APPLE__) // GLFW >= 3.4 only; macOS has no X11
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

  // The cursor is free until the game starts (the first thing to do is to type the server's address)
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

  return window;
}


// The game's client: the window. It asks where the server is, connects, makes the same map as the
// server and shows it: the objects of the world are copies that move as the server says (see
// net/NetClient), the player's own penguin moves at once with his controls and is corrected
// from what the server says. What he does (his controls, E, F, R...) is sent to the server,
// which carries it out. Sound, speech and everything that is only drawn or heard is the client's.

int main(int argc, char **argv) {
  FloorMode floorMode = FloorMode::HeightField; // pass --ray for DownwardRay
  bool profile = false;      // --profile: print where the time of each frame goes
  std::string connectTo;     // --connect ADDRESS: connect at once, without asking
  std::string playerName;    // --name NAME
  for (int i = 1; i < argc; i++)
    if (std::string(argv[i]) == "--windowed")
      FULLSCREEN = false;
    else if (std::string(argv[i]) == "--ray")
      floorMode = FloorMode::DownwardRay;
    else if (std::string(argv[i]) == "--connect" && i + 1 < argc)
      connectTo = argv[++i];
    else if (std::string(argv[i]) == "--name" && i + 1 < argc)
      playerName = argv[++i];
    else if (std::string(argv[i]) == "--profile")
      profile = true;
    else {
      printf("Uso: %s [--windowed] [--connect DIRECCION[:PUERTO]] [--name NOMBRE] [--ray] [--profile]\n", argv[0]);
      return std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h" ? 0 : 1;
    }
  if (!enterSourceDir())
    fprintf(stderr, "Could not find src/, using the current directory\n");
  setNetRole(NetRole::Client);

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
  // The rear-view mirrors of the vehicle (three: the two side ones, tall, and the central one, wide):
  // a camera of its own draws the world behind into each one's texture (see the main loop, where
  // they take turns); the vehicle's glass shows it
  const int MIRRORS = 3;
  const int MIRROR_SIZE[MIRRORS][2] = {{320, 640}, {320, 640}, {520, 160}}; // pixels, width x height
  GLuint mirrorTexture[MIRRORS] = {0, 0, 0}, mirrorFbo[MIRRORS] = {0, 0, 0}, mirrorDepth = 0;
  bool mirrorWorks = true;
  glGenRenderbuffers(1, &mirrorDepth); // (one depth buffer for all: they are never drawn together;
  glBindRenderbuffer(GL_RENDERBUFFER, mirrorDepth); // as wide and as tall as the biggest of them)
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 520, 640);
  for (int i = 0; i < MIRRORS; i++) {
    glGenTextures(1, &mirrorTexture[i]);
    glBindTexture(GL_TEXTURE_2D, mirrorTexture[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, MIRROR_SIZE[i][0], MIRROR_SIZE[i][1], 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &mirrorFbo[i]);
    glBindFramebuffer(GL_FRAMEBUFFER, mirrorFbo[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mirrorTexture[i], 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mirrorDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      mirrorWorks = false;
  }
  if (!mirrorWorks)
    fprintf(stderr, "The mirrors' framebuffer is not complete: no mirrors\n");
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  Camera mirrorCamera(window, &shader); // (its aspect is each mirror's, when it draws it)

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
  // Debug: select objects and see their data (key 1, see DebugSelector). Moving things and
  // changing their values are not allowed here: the world is the server's
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
  // Controlled by the Flatwoods monster: the screen goes dark red (the same kind of tint)
  DeathOverlay possessionOverlay;
  possessionOverlay.setColor(vec3(0.25f, 0.0f, 0.02f));
  ui.addOverlay(&possessionOverlay);
  // Held by Bob: which key to hammer to get free, and how near he is
  StruggleOverlay struggleOverlay([&controls]() { return controls.keyName(Action::LeaveVehicle); });
  ui.addOverlay(&struggleOverlay);
  // Aiming Bob's ship's ray gun: a crosshair in the middle
  CrosshairOverlay crosshair;
  ui.addOverlay(&crosshair);
  // The other players' names, and the server's messages
  NetOverlay netOverlay;
  ui.addOverlay(&netOverlay);
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

  const std::vector<MapEntry> &maps = mapList();
  std::unique_ptr<NetClient> net;     // the connection (null: not connecting)
  std::unique_ptr<GameStage> stage;   // the world, once connected (null: the connect screen)
  ConnectMenu *connectMenu = nullptr; // (owned by the UI)
  std::string lastAddress = connectTo.empty() ? settings.getString("net.server", "127.0.0.1") : connectTo;
  if (playerName.empty()) {
    const char *user = getenv("USER");
    playerName = settings.getString("net.name", user && *user ? user : "Pingu");
  }
  bool autoConnect = !connectTo.empty(); // (once: --connect)

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

  // --- The connect screen
  auto quit = [window]() { glfwSetWindowShouldClose(window, true); };
  auto openConnectMenu = [&](const std::string &status) {
    ui.closeAll();
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    connectMenu = static_cast<ConnectMenu *>(ui.open(new ConnectMenu(
        lastAddress, playerName,
        [&](const std::string &address, const std::string &name) {
          lastAddress = address;
          playerName = name.empty() ? "Pingu" : name;
          net.reset(new NetClient(lastAddress, playerName));
          connectMenu->setBusy(true);
          connectMenu->setStatus("Conectando con " + lastAddress + "...");
        },
        quit)));
    connectMenu->setStatus(status);
  };

  // The map the server said is made and attached to the connection: the game starts
  auto startGame = [&]() -> std::string {
    int index = net->getMapIndex();
    if (index < 0 || index >= (int)maps.size())
      return "El servidor usa un mapa que este cliente no tiene";
    MapContext context = {floorMode, sound, speech};
    std::unique_ptr<GameStage> next = maps[index].create(context);
    if (!next)
      return "No puedo cargar el mapa '" + maps[index].name + "'";
    if (!net->attach(*next))
      return net->getError();
    ui.closeAll();
    connectMenu = nullptr;
    interaction.clear();
    selector.clear();
    stage = std::move(next);
    // Each map brings its own music (or none, which silences the previous)
    music.play(stage->getMusic(), stage->isMusicLooping(), stage->getMusicVolume());
    ambience.play(stage->getAmbience(), true, stage->getAmbienceVolume());
    const std::vector<Interactable *> &targets = stage->getInteractables();
    for (Interactable *object : targets)
      interaction.add(object);
    // Things that act at once (the RV's door, the ship's ramp) are done by the server
    interaction.setDirectUse([&](Interactable &target) {
      const std::vector<Interactable *> &list = stage->getInteractables();
      for (size_t i = 0; i < list.size(); i++)
        if (list[i] == &target)
          net->sendUse((unsigned)i, stage->getPlayer()->getPosition());
    });
    stage->setRefuelRequest([&]() { net->sendAction(Net::A_REFUEL); });
    camera.setFarPlane(stage->getFarPlane());
    mirrorCamera.setFarPlane(stage->getFarPlane());
    if (mirrorWorks)
      for (int i = 0; i < MIRRORS; i++)
        stage->setRearMirrorTexture(i, mirrorTexture[i], (float)MIRROR_SIZE[i][0] / MIRROR_SIZE[i][1]);
    deathTime = -1.0;
    credits.stop();
    scream.reset();
    rip.reset();
    // The mouse is the camera's now (the controller takes it when it is enabled again)
    controller.setEnabled(false);
    controller.setEnabled(true);
    settings.setString("net.server", lastAddress);
    settings.setString("net.name", playerName);
    settings.save();
    printf("Conectado a %s: mapa '%s', jugador %d\n", lastAddress.c_str(), maps[index].name.c_str(), net->getPlayerId());
    return std::string();
  };

  // Back to the connect screen (the server went, or the connection was lost)
  auto leaveGame = [&](const std::string &why) {
    controller.detach();
    ui.setHint("");
    camera.attachTo(nullptr, 0.0f, 0.0f);
    camera.setCarrier(mat3(1.0f));
    interaction.clear();
    selector.turnOff();
    selector.clear();
    music.play(nullptr);
    ambience.play(nullptr);
    scream.reset();
    rip.reset();
    credits.stop();
    deathOverlay.setAmount(0.0f);
    paralysisOverlay.setAmount(0.0f);
    struggleOverlay.setProgress(-1.0f);
    crosshair.setShown(false);
    netOverlay.setTags({});
    stage.reset();
    net.reset();
    openConnectMenu(why);
    printf("%s\n", why.c_str());
  };

  // Keys with no panel open: Esc shows the pause menu (whose "Salir" / Quit
  // key ends the game)
  bool disconnectRequested = false; // (done at the start of the next frame, not inside the menu's button)
  MenuContext menus = {ui, camera, controls, settings, sound, quit, [&]() { disconnectRequested = true; }};
  ui.bindKey(GLFW_KEY_ESCAPE, [&]() {
    if (stage)
      ui.open(new PauseMenu(menus));
  });

  // Debug select key (1): turns the object selection mode on and off
  ui.bindKey([&controls]() { return controls.key(Action::DebugSelect); },
             [&]() {
               if (stage)
                 selector.toggleSelect();
             });

  // The keys that act in the world are the server's business: they are sent to it. (Not while the
  // debug selection mode uses the mouse, nor when the player is not in a game.)
  auto act = [&](Action key, uint8_t what) {
    ui.bindKey([&controls, key]() { return controls.key(key); }, [&, what]() {
      if (stage && net && !stage->isPlayerDead() && !selector.capturesMouse())
        net->sendAction(what);
    });
  };
  // Leave-vehicle key: the player gets out, or hammers it to get free of Bob
  act(Action::LeaveVehicle, Net::A_LEAVE);
  // Headlights key: the flashlight, the vehicle's lights or the ship's gun
  act(Action::Headlights, Net::A_HEADLIGHTS);
  // Engine key
  act(Action::Engine, Net::A_ENGINE);
  // Handbrake key
  act(Action::Handbrake, Net::A_HANDBRAKE);
  // Vehicle camera key: inside the vehicle or from behind
  act(Action::VehicleCamera, Net::A_VEHICLE_CAMERA);
  // Ship-legs key: flying Bob's ship, its legs go in or out
  act(Action::ShipLegs, Net::A_SHIP_LEGS);

  // The command console (key T): what is typed goes to the server, which knows the commands
  Commands commands;
  auto sendToServer = [&](const std::vector<std::string> &) {
    return std::string("Enviado al servidor");
  };
  for (const char *name : {"day", "night", "time"})
    commands.add(name, "se lo pide al servidor", sendToServer);
  std::vector<std::string> commandHistory;
  auto openConsole = [&](const std::string &text) {
    if (!stage)
      return;
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    const float MARGIN = 16.0f;
    UIPanel *console = ui.open(new CommandConsole(
        commands, commandHistory, width - 2 * MARGIN, text, [&](const std::string &line) { net->sendCommand(line); }));
    console->moveTo(MARGIN, height - console->preferredHeight() - MARGIN);
  };
  ui.bindKey([&controls]() { return controls.key(Action::Console); },
             [&]() { openConsole(""); });
  // Typing '/' (whatever key it is on the layout) opens it with the '/' of a
  // command already written
  ui.bindChar('/', [&]() { openConsole("/"); });

  openConnectMenu("");
  if (autoConnect) {
    net.reset(new NetClient(lastAddress, playerName));
    connectMenu->setBusy(true);
    connectMenu->setStatus("Conectando con " + lastAddress + "...");
  }

  // The player's controls in the last input sent (the number the server will answer with)
  unsigned lastInputSeq = 0;

  // Main loop
  double lastTime = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    double now = glfwGetTime();
    double dt = std::min(now - lastTime, 0.25);
    lastTime = now;
    camera.resize();

    // What the server sends (and what we send it)
    if (net) {
      net->update();
      for (const std::string &text : net->takeNotices())
        netOverlay.showNotice(text);
    }
    netOverlay.update((float)dt);

    // --- The connect screen: until the server accepts us and the map is ready
    if (!stage) {
      if (net && net->isFailed()) {
        std::string why = net->getError();
        net.reset();
        if (!ui.isOpen(connectMenu))
          openConnectMenu(why);
        connectMenu->setBusy(false);
        connectMenu->setStatus(why);
      } else if (net && net->isReady()) {
        connectMenu->setStatus("Cargando el mapa...");
        std::string problem = startGame();
        if (!problem.empty()) {
          net.reset();
          connectMenu->setBusy(false);
          connectMenu->setStatus(problem);
        } else {
          lastTime = glfwGetTime(); // (loading took a while: not one huge step of the physics)
          continue;
        }
      }
      ui.update();
      glClearColor(0.05f, 0.07f, 0.12f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      ui.draw();
      glfwSwapBuffers(window);
      glfwPollEvents();
      continue;
    }

    // --- In the game
    if (disconnectRequested) {
      disconnectRequested = false;
      leaveGame("Te has desconectado del servidor");
      continue;
    }
    if (net->isFailed()) {
      leaveGame(net->getError().empty() ? "Desconectado del servidor" : net->getError());
      continue;
    }
    // The world as the server has it now
    net->apply(dt);
    Player *me = stage->getLocalPlayer();

    // The interface first: while any panel (menu or object) is open, the
    // player's controls are paused and the cursor is free
    interaction.update(stage->getPlayer()->getPosition(),
                       stage->interactionsEnabled());
    ui.update();
    // The server handed the controls to another character (got in or out of a
    // vehicle): the controller and the camera follow it
    if (stage->takePlayerChange())
      controller.attach(stage->getPlayer().get(), stage->getCameraDistance(),
                        stage->getCameraHeight(), stage->getCameraYaw());
    controller.setEnabled(!ui.hasPanels() && !stage->isPlayerDead() && !stage->playerImmobilized());
    paralysisOverlay.setAmount(0.35f * stage->playerParalysis() * (0.8f + 0.2f * (float)std::sin(now * 9.0)));
    struggleOverlay.setProgress(stage->struggleProgress());
    struggleOverlay.setPossessed(stage->playerPossessed());
    possessionOverlay.setAmount(stage->playerPossessed() ? 0.3f + 0.08f * (float)std::sin(now * 2.0) : 0.0f);
    crosshair.setShown(stage->playerAiming() && !stage->isPlayerDead());
    controller.setLookEnabled(!selector.capturesMouse());
    controller.update();
    // Controlled by the Flatwoods monster: the view turns slowly to where his body walks, level
    float possessedYaw;
    if (stage->possessedLook(possessedYaw)) {
      float turn = std::remainder(possessedYaw - camera.getYaw(), 6.2831853f);
      float step = 1.5f * (float)dt;
      camera.setAngles(camera.getYaw() + glm::clamp(turn, -step, step),
                       camera.getPitch() + glm::clamp(0.1f - camera.getPitch(), -step, step));
    }
    // The controls go to the server
    {
      unsigned sent = net->sendInput(dt, controller.getMove(), controller.getUp(), controller.getYaw(),
                                     controller.getPitch(), controller.isRunning());
      if (sent)
        lastInputSeq = sent;
    }
    // The left button fires (Bob's ship's ray gun), once per press: not over a panel, nor in a
    // debug mode (they use the mouse)
    {
      static bool fireWasDown = false;
      bool fireDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
      if (fireDown && !fireWasDown && !ui.hasPanels() && selector.getMode() == DebugSelector::Mode::Off &&
          !stage->isPlayerDead() && stage->playerAiming())
        net->sendFire(camera.getPosition(), camera.getForward());
      fireWasDown = fireDown;
    }
    stage->setViewer(camera.getPosition(), camera.getViewProjection()); // (as it was the last frame)
    double tUpdate = glfwGetTime();
    stage->update(dt);
    double tPhysics = glfwGetTime() - tUpdate;
    if (me && me->character == me->walker && !me->dead)
      net->recordPrediction(lastInputSeq, me->walker->getPosition());
    // a map may fade its sounds (with the time of day)
    music.setVolume(stage->getMusicVolume());
    ambience.setVolume(stage->getAmbienceVolume());
    stage->getPlayer()->followCamera();
    // The server brought him back from the dead
    if (deathTime >= 0.0 && !stage->isPlayerDead()) {
      deathTime = -1.0;
      credits.stop();
      scream.reset();
      rip.reset();
      camera.setCarrier(mat3(1.0f));
      deathOverlay.setColor(vec3(0.7f, 0.0f, 0.02f));
    }
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
      (void)CREDITS_PAUSE; // (no credits: the server brings him back soon)
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
      (void)CREDITS_PAUSE; // (no credits: the server brings him back soon)
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
    // A mirror's picture first (if the vehicle has one to show now): the world seen from the glass,
    // into its texture; then everything back as it was for the real view. The mirrors take turns, one
    // each frame, so that all together cost what one would (each is two frames late at most)
    static unsigned mirrorTurn = 0;
    int mirrorSide = (int)(mirrorTurn++ % MIRRORS);
    MirrorView mirror;
    if (mirrorWorks && stage->rearMirror(mirrorSide, mirror)) {
      stage->showRearMirror(mirrorSide, false);
      glBindFramebuffer(GL_FRAMEBUFFER, mirrorFbo[mirrorSide]);
      glViewport(0, 0, MIRROR_SIZE[mirrorSide][0], MIRROR_SIZE[mirrorSide][1]);
      glClearColor(horizon.x, horizon.y, horizon.z, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      vec3 right = glm::normalize(glm::cross(mirror.forward, mirror.up));
      vec3 up = glm::cross(right, mirror.forward);
      mirrorCamera.setAspect(mirror.aspect);
      mirrorCamera.setFov(mirror.fov);
      mirrorCamera.setCarrier(mat3(right, up, -mirror.forward));
      mirrorCamera.reposition(mirror.position.x, mirror.position.y, mirror.position.z);
      shader.setFloat("time", (float)now);
      stage->setMirrorView(true); // (what only shows in mirrors)
      stage->render(&shader, mirror.position, now);
      stage->setMirrorView(false);
      const Environment &mirrorEnv = stage->getEnvironment();
      particles.setLighting(mirrorEnv.lightColor, mirrorEnv.lightDir, lights);
      particles.draw(stage->getEmitters(), mirrorCamera);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      int fbw, fbh;
      glfwGetFramebufferSize(window, &fbw, &fbh);
      glViewport(0, 0, fbw, fbh);
      shader.use();
      camera.update(); // (the real camera's matrices to the shader again)
      stage->showRearMirror(mirrorSide, true);
    }
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
    {
      // The other players' names above their heads
      std::vector<NetOverlay::Tag> tags;
      mat4 viewProjection = camera.getViewProjection();
      for (const auto &p : stage->getPlayers()) {
        if (p.get() == stage->getLocalPlayer() || p->dead || p->abducted)
          continue;
        vec3 at = p->walker->getPosition() + vec3(0.0f, p->inVehicle || p->inSaucer || p->seated ? 3.8f : 2.2f, 0.0f);
        if (length(at - camera.getPosition()) > 80.0f)
          continue;
        vec4 clip = viewProjection * vec4(at, 1.0f);
        if (clip.w <= 0.1f)
          continue;
        vec2 ndc = vec2(clip.x, clip.y) / clip.w;
        if (std::fabs(ndc.x) < 1.1f && std::fabs(ndc.y) < 1.1f)
          tags.push_back({p->name, ndc});
      }
      netOverlay.setTags(tags);
    }
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

  stage.reset();
  net.reset();
  glfwTerminate();
  return 0;
}
