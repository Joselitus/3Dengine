#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "AnimatedMesh.h"
#include "AnimatedModel.h"
#include "Camera.h"
#include "Controller.h"
#include "GameObject.h"
#include "Light.h"
#include "Model.h"
#include "Shader.h"
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

int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++)
    if (std::string(argv[i]) == "--windowed")
      FULLSCREEN = false;


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

  // Night sky. moonDir must match MOON_DIR in assets/sky/generate_sky.py
  const vec3 moonDir = normalize(vec3(-0.32f, 0.26f, -0.91f));
  const vec3 horizon = vec3(0.035f, 0.055f, 0.110f);

  // Creation of light
  Light light(0.28f, 0.34f, 0.62f, &shader); // cool, dim moonlight
  light.moveTo(moonDir.x * 100, moonDir.y * 100, moonDir.z * 100); // moon

  Model skydome("../assets/sky/skydome.obj");
  GameObject sky(&skydome);
  shader.setVector3("moonDir", moonDir.x, moonDir.y, moonDir.z);
  shader.setVector3("fogColor", horizon.x, horizon.y, horizon.z);
  shader.setInt("unlit", 0);

  // Desert scene
  const float GROUND_Y = -1.0f; // the penguin's feet are at about -0.9
  Model dunes("../assets/desert/dunes.obj");
  Model cactusA("../assets/desert/cactus_a.obj");
  Model cactusB("../assets/desert/cactus_b.obj");
  Model rockA("../assets/desert/rock_a.obj");
  Model rockB("../assets/desert/rock_b.obj");

  // x, z, terrain height at (x, z) (see dune_height in generate_assets.py),
  // rotation around y, uniform scale
  struct Prop {
    Model *model;
    float x, z, height, yaw, scale;
  };
  const Prop propList[] = {
      {&cactusA, -4.5f, -8, 0.35f, 0.5f, 1.0f},
      {&cactusA, 8, -16, 1.60f, 2.0f, 1.3f},
      {&cactusA, -16, -24, 4.61f, 4.0f, 1.6f},
      {&cactusA, 18, -30, 1.17f, 1.0f, 1.2f},
      {&cactusB, 5.5f, -10, 0.97f, 3.0f, 1.1f},
      {&cactusB, -9, -14, 1.61f, 5.5f, 1.2f},
      {&cactusB, 14, -22, 1.62f, 0.8f, 1.5f},
      {&rockA, -4, -6, 0.11f, 0.3f, 1.0f},
      {&rockA, 11, -9, 2.23f, 2.5f, 1.8f},
      {&rockA, -20, -12, 1.90f, 4.5f, 2.5f},
      {&rockB, 3.5f, -4.5f, 0.02f, 1.2f, 1.0f},
      {&rockB, -8, -11, 1.03f, 3.3f, 1.4f},
      {&rockB, 7, -18, 1.52f, 5.0f, 1.7f},
      {&rockB, -3, -14, 1.23f, 0.1f, 1.1f},
  };
  // The creature, standing in the distance and facing the camera
  Model creatureBody("../assets/creature/creature.obj");
  Model creatureEyes("../assets/creature/creature_eyes.obj");
  GameObject creature(&creatureBody);
  GameObject creatureEyesObj(&creatureEyes);
  {
    const float x = 1.5f, z = -13.0f, height = 0.91f; // see dune_height()
    mat4 turn = glm::rotate(mat4(1.0), 0.25f, vec3(0, 1, 0));
    for (GameObject *o : {&creature, &creatureEyesObj}) {
      o->setPosition(x, GROUND_Y + height, z);
      o->setRotation(turn);
    }
  }

  GameObject ground(&dunes);
  ground.setPosition(0.0f, GROUND_Y, 0.0f);
  std::vector<GameObject> props;
  for (const Prop &p : propList) {
    GameObject o(p.model);
    // sink the base a little so nothing floats on the slopes
    o.setPosition(p.x, GROUND_Y + p.height - 0.05f, p.z);
    o.setRotation(glm::scale(glm::rotate(mat4(1.0), p.yaw, vec3(0, 1, 0)),
                             vec3(p.scale)));
    props.push_back(o);
  }

  AnimatedModel model("../assets/ping/PenguinoAnimado.fbx");
  GameObject ping(&model);

  // The controller drives the penguin; the camera follows behind it
  controller.attach(&ping, 4.0f, 0.8f);

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    // Clear the screen. It can cause flickering, so it's there nonetheless.
    camera.resize();
    controller.update();
    glClearColor(horizon.x, horizon.y, horizon.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Update animation
    model.Update(glfwGetTime());

    // Draw
    // The sky is centred on the camera and drawn first, without depth
    glDisable(GL_DEPTH_TEST);
    shader.setInt("unlit", 1);
    shader.setFloat("time", (float)glfwGetTime());
    vec3 cam = camera.getPosition();
    sky.setPosition(cam.x, cam.y, cam.z);
    sky.Draw(&shader);
    shader.setInt("unlit", 0);
    glEnable(GL_DEPTH_TEST);

    ground.Draw(&shader);
    for (GameObject &o : props)
      o.Draw(&shader);
    shader.setFloat("breathTime", (float)glfwGetTime());
    shader.setFloat("breathAmp", 2.0f); // the creature breathes
    creature.Draw(&shader);
    shader.setInt("unlit", 2); // the eyes glow
    creatureEyesObj.Draw(&shader);
    shader.setInt("unlit", 0);
    shader.setFloat("breathAmp", 0.0f);
    ping.Draw(&shader);

    // Swap buffers
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}
