#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "AnimatedMesh.h"
#include "AnimatedModel.h"
#include "Camera.h"
#include "Controller.h"
#include "GameObject.h"
#include "Light.h"
#include "PlayableCharacter.h"
#include "RV.h"
#include "Model.h"
#include "Shader.h"
#include "Stage.h"
#include "Skeleton.h"
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

// The desert: dunes with a road winding through them, cacti and rocks, and
// the RV that the player drives.
class TestStage : public Stage {
private:
  static constexpr float GROUND_Y = -1.0f; // ground level of the clearing
  std::shared_ptr<RV> rv;

protected:
  // The RV stays on the dunes
  void apply(DynamicGameObject &object, double dt) override {
    collideWithFloor(object);
  }

public:
  TestStage(FloorMode mode) : Stage(mode) {
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
  }

  std::shared_ptr<PlayableCharacter> getPlayer() { return rv; }
};

int main(int argc, char **argv) {
  FloorMode floorMode = FloorMode::HeightField; // pass --ray for DownwardRay
  for (int i = 1; i < argc; i++)
    if (std::string(argv[i]) == "--windowed")
      FULLSCREEN = false;
    else if (std::string(argv[i]) == "--ray")
      floorMode = FloorMode::DownwardRay;


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
  Controller controller(window, &camera);

  // Daylight: plain blue sky (the clear colour), distant geometry fades into it
  const vec3 sunDir = normalize(vec3(-0.3f, 0.8f, -0.5f));
  const vec3 horizon = vec3(0.45f, 0.68f, 0.92f);

  // Creation of light
  Light light(0.85f, 0.83f, 0.78f, &shader); // warm white sunlight
  light.moveTo(sunDir.x * 100, sunDir.y * 100, sunDir.z * 100); // sun

  shader.setVector3("moonDir", sunDir.x, sunDir.y, sunDir.z);
  shader.setVector3("fogColor", horizon.x, horizon.y, horizon.z);
  shader.setInt("unlit", 0);

  TestStage stage(floorMode);

  // The controller steers the RV; the camera follows behind it
  controller.attach(stage.getPlayer().get(), 12.0f, 3.5f);

  // Main loop
  double lastTime = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    // Clear the screen. It can cause flickering, so it's there nonetheless.
    double now = glfwGetTime();
    double dt = now - lastTime;
    lastTime = now;

    camera.resize();
    controller.update();
    stage.update(dt);
    stage.getPlayer()->followCamera();
    glClearColor(horizon.x, horizon.y, horizon.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw (the clear colour is the sky)
    shader.setFloat("time", (float)now);
    stage.Draw(&shader, now);

    // Swap buffers
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}
