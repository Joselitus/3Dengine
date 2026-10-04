#include <GL/glew.h>
#include <climits>
#include <cstdlib>
#include <unistd.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "AnimatedMesh.h"
#include "AnimatedModel.h"
#include "Camera.h"
#include "Controller.h"
#include "GameObject.h"
#include "Light.h"
#include "Model.h"
#include "Scene.h"
#include "Shader.h"
#include "Skeleton.h"
#include "myopengl.h"

using namespace std;
using namespace glm;

// Usage: test [--windowed] [scene file]   (from any directory)
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
  std::string scenePath = "../assets/scenes/desert.scene"; // from src/
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--windowed")
      FULLSCREEN = false;
    else {
      // Relative to the launch directory, so resolve it before moving
      char resolved[PATH_MAX];
      if (!realpath(argv[i], resolved)) {
        fprintf(stderr, "%s: scene file not found\n", argv[i]);
        return -1;
      }
      scenePath = resolved;
    }
  }
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
  Controller controller(window, &camera);

  // The layout of the world lives in a data file, shared with the web viewer
  // in tools/scene_viewer
  Scene scene;
  if (!scene.load(scenePath, "../assets")) {
    glfwTerminate();
    return -1;
  }
  const SceneFile &info = scene.info();

  // The moon is the only light; far enough away to act as a directional one
  Light light(info.lightColor.r, info.lightColor.g, info.lightColor.b,
              &shader);
  light.moveTo(info.moonDir.x * 100, info.moonDir.y * 100,
               info.moonDir.z * 100);

  // The controller drives the player; the camera follows behind it
  if (scene.getPlayer())
    controller.attach(scene.getPlayer(), info.cameraDistance,
                      info.cameraHeight);

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    camera.resize();
    controller.update();
    // Clear to the horizon colour, so any gap blends with the sky and fog
    glClearColor(info.fogColor.r, info.fogColor.g, info.fogColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    double now = glfwGetTime();
    scene.Update(now);
    scene.Draw(&shader, camera.getPosition(), (float)now);

    // Swap buffers
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}
