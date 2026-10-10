// The cloud debugger: one cloud (a box, see CloudRenderer) over an empty floor, with sliders for
// everything that shapes it: the 3D Worley texture (cells of each octave, seed, resolution,
// weights; "Regenerar textura" bakes it again), where the box looks into the texture (offset,
// tiling per axis, drift), the density (threshold, absorption, edge fade) and the box itself (size and
// position). A grey cube (a stand-in for the scene's solid objects, with its own sliders) shows
// how the cloud is cut by the depth buffer.
//   ../test/cloud_debugger
// Left button (not over a panel): orbit the camera; wheel: zoom; W A S D / Q E: move the point
// it orbits (horizontally / down and up); Esc: quit.
#include <GL/glew.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>
#include <GLFW/glfw3.h>

#include "Camera.h"
#include "CloudRenderer.h"
#include "GameStage.h"
#include "Light.h"
#include "MaterialMap.h"
#include "Shader.h"
#include "UIButton.h"
#include "UILabel.h"
#include "UIManager.h"
#include "UIPanel.h"
#include "UISlider.h"

using namespace std;
using namespace glm;

namespace {

const int WIDTH = 1280, HEIGHT = 760;
const float PANEL_WIDTH = 280.0f, MARGIN = 10.0f;

// The empty space: a flat floor and a bit of light
class ToolStage : public GameStage {
public:
  ToolStage() : GameStage(FloorMode::HeightField) {
    environment.lightDir = normalize(vec3(0.4f, 0.8f, 0.5f));
    environment.horizon = vec3(0.55f, 0.72f, 0.92f); // a sky for the clouds to stand against
    const char *floorFile = "../tools/creature_testing/floor/floor.obj";
    auto ground = make_shared<GameObject>(loadModel(floorFile));
    ground->setCollidable(false);
    add(ground);
    setFloor(loadModel(floorFile), vec3(0.0f), MaterialMap::uniform(FloorMaterial::Sand));
  }
};

// A panel that stays open: it takes every key (Esc asks to quit instead)
class ToolPanel : public UIPanel {
  function<void()> quit;

public:
  ToolPanel(const string &title, function<void()> quit) : UIPanel(title, PANEL_WIDTH, false), quit(quit) {}
  bool onKey(int key) override {
    if (key == GLFW_KEY_ESCAPE)
      quit();
    return true;
  }
};

float scrollSum = 0.0f;
void onScroll(GLFWwindow *, double, double dy) { scrollSum += (float)dy; }

bool enterSourceDir() {
  char exe[PATH_MAX];
  ssize_t length = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
  if (length <= 0)
    return false;
  exe[length] = '\0';
  string dir(exe);
  dir = dir.substr(0, dir.find_last_of('/')) + "/../src";
  return chdir(dir.c_str()) == 0;
}

GLFWwindow *openWindow() {
  glewExperimental = true;
#if defined(GLFW_PLATFORM) && !defined(__APPLE__)
  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return nullptr;
  }
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "cloud debugger", NULL, NULL);
  if (!window) {
    fprintf(stderr, "Failed to open the window\n");
    glfwTerminate();
    return nullptr;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "Failed to initialize GLEW\n");
    return nullptr;
  }
  return window;
}

} // namespace

int main(int argc, char **argv) {
  if (argc > 1) {
    fprintf(stderr, "Usage: %s (no options)\n", argv[0]);
    return std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h" ? 0 : 1;
  }
  if (!enterSourceDir())
    fprintf(stderr, "Could not find src/, using the current directory\n");
  GLFWwindow *window = openWindow();
  if (!window)
    return -1;
  glEnable(GL_DEPTH_TEST);
  glfwSetScrollCallback(window, onScroll);

  Shader shader("shaders/animatedshader.vert", "shaders/shader.frag");
  Camera camera(window, &shader);
  Light light(1.0f, 1.0f, 1.0f, &shader);
  shader.setInt("unlit", 0);
  UIManager ui(window);
  CloudRenderer clouds;
  ToolStage stage;

  // The reference cube, 8 m a side
  const float CUBE_SIDE = 8.0f;
  auto cube = make_shared<GameObject>(stage.loadModel("../assets/cube/cube.obj"));
  cube->setScale(CUBE_SIDE);
  cube->setCollidable(false);
  stage.add(cube);

  // The cloud's box (centre and size) and the cube's place: what the sliders edit
  const vec3 DEFAULT_CENTRE(0.0f, 160.0f, 0.0f), DEFAULT_SIZE(6000.0f, 80.0f, 6000.0f); // the game's sky layer
  vec3 centre = DEFAULT_CENTRE, size = DEFAULT_SIZE;
  vec3 cubeAt(0.0f, 0.0f, 0.0f);

  // ---- the sliders
  float sunHeight = 1.0f; // the sun's elevation, as Environment::sunDir.y
  bool quit = false;
  auto askQuit = [&]() { quit = true; };
  auto slider = [](UIPanel *p, const char *label, float lo, float hi, float step, float &value,
                   const char *unit = "") {
    p->add(new UISlider(label, lo, hi, step, [&value]() { return value; },
                        [&value](float v) { value = v; }, unit));
  };
  // (the settings' ints, as floats for a slider: they are read back by the lambdas)
  CloudSettings &s = clouds.settings;
  const CloudSettings defaults = s;
  auto intSlider = [](UIPanel *p, const char *label, float lo, float hi, int &value) {
    p->add(new UISlider(label, lo, hi, 1.0f, [&value]() { return (float)value; },
                        [&value](float v) { value = (int)std::lround(v); }));
  };

  ToolPanel *texturePanel = new ToolPanel("Textura (Worley 3D)", askQuit);
  intSlider(texturePanel, "Celdas, octava R", 1, 32, s.cells[0]);
  intSlider(texturePanel, "Celdas, octava G", 1, 32, s.cells[1]);
  intSlider(texturePanel, "Celdas, octava B", 1, 32, s.cells[2]);
  intSlider(texturePanel, "Semilla", 0, 50, s.seed);
  intSlider(texturePanel, "Resolucion (texels)", 16, 128, s.resolution);
  slider(texturePanel, "Peso R", 0.0f, 1.0f, 0.0f, s.weights.x);
  slider(texturePanel, "Peso G", 0.0f, 1.0f, 0.0f, s.weights.y);
  slider(texturePanel, "Peso B", 0.0f, 1.0f, 0.0f, s.weights.z);
  texturePanel->add(new UILabel("Celdas, semilla y resolucion:"));
  texturePanel->add(new UIButton("Regenerar textura", [&]() { clouds.regenerate(); }));

  ToolPanel *lookPanel = new ToolPanel("Aspecto y desplazamiento", askQuit);
  slider(lookPanel, "Umbral de densidad", 0.0f, 1.0f, 0.0f, s.threshold);
  slider(lookPanel, "Absorcion", 0.0f, 0.5f, 0.0f, s.absorption);
  slider(lookPanel, "Borde que se difumina", 0.0f, 0.5f, 0.0f, s.edgeFade);
  slider(lookPanel, "Repeticiones x", 0.25f, 40.0f, 0.0f, s.tiling.x);
  slider(lookPanel, "Repeticiones y", 0.25f, 40.0f, 0.0f, s.tiling.y);
  slider(lookPanel, "Repeticiones z", 0.25f, 40.0f, 0.0f, s.tiling.z);
  slider(lookPanel, "Desplazamiento X", 0.0f, 1.0f, 0.0f, s.offset.x);
  slider(lookPanel, "Desplazamiento Y", 0.0f, 1.0f, 0.0f, s.offset.y);
  slider(lookPanel, "Desplazamiento Z", 0.0f, 1.0f, 0.0f, s.offset.z);
  slider(lookPanel, "Deriva X (por s)", -0.05f, 0.05f, 0.0f, s.drift.x);
  slider(lookPanel, "Deriva Z (por s)", -0.05f, 0.05f, 0.0f, s.drift.z);
  lookPanel->add(new UIButton("Valores por defecto", [&]() {
    clouds.settings.weights = defaults.weights;
    clouds.settings.offset = defaults.offset;
    clouds.settings.drift = defaults.drift;
    clouds.settings.tiling = defaults.tiling;
    clouds.settings.threshold = defaults.threshold;
    clouds.settings.absorption = defaults.absorption;
    clouds.settings.edgeFade = defaults.edgeFade;
  }));

  lookPanel->add(new UISlider("Altura del sol (color)", -0.4f, 1.0f, 0.0f, [&]() { return sunHeight; },
                              [&](float v) { sunHeight = v; }));

  ToolPanel *boxPanel = new ToolPanel("Caja de nube y cubo", askQuit);
  slider(boxPanel, "Ancho (x)", 2.0f, 8000.0f, 0.0f, size.x, " m");
  slider(boxPanel, "Alto (y)", 2.0f, 400.0f, 0.0f, size.y, " m");
  slider(boxPanel, "Fondo (z)", 2.0f, 8000.0f, 0.0f, size.z, " m");
  slider(boxPanel, "Centro x", -4000.0f, 4000.0f, 0.0f, centre.x, " m");
  slider(boxPanel, "Centro y", 0.0f, 400.0f, 0.0f, centre.y, " m");
  slider(boxPanel, "Centro z", -4000.0f, 4000.0f, 0.0f, centre.z, " m");
  slider(boxPanel, "Cubo x", -60.0f, 60.0f, 0.0f, cubeAt.x, " m");
  slider(boxPanel, "Cubo y (base)", 0.0f, 40.0f, 0.0f, cubeAt.y, " m");
  slider(boxPanel, "Cubo z", -60.0f, 60.0f, 0.0f, cubeAt.z, " m");
  boxPanel->add(new UIButton("Caja pequena (40 m)", [&]() { centre = vec3(0.0f, 14.0f, 0.0f); size = vec3(40.0f, 16.0f, 40.0f); }));
  boxPanel->add(new UIButton("Cielo completo (por defecto)", [&]() { centre = DEFAULT_CENTRE; size = DEFAULT_SIZE; }));

  ui.open(texturePanel);
  texturePanel->moveTo(MARGIN, MARGIN);
  ui.open(lookPanel);
  lookPanel->moveTo(2 * MARGIN + PANEL_WIDTH, MARGIN);
  ui.open(boxPanel);
  boxPanel->moveTo(WIDTH - PANEL_WIDTH - MARGIN, MARGIN);
  ui.bindKey(GLFW_KEY_ESCAPE, askQuit);

  float yaw = 0.6f, pitch = -0.45f, distance = 20.0f; // looking up at the layer
  vec3 focus(0.0f, 60.0f, 0.0f);
  bool orbiting = false;
  double lastX = 0.0, lastY = 0.0, lastTime = glfwGetTime();
  while (!glfwWindowShouldClose(window) && !quit) {
    double now = glfwGetTime();
    float dt = (float)std::min(now - lastTime, 0.1);
    lastTime = now;
    camera.resize();

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    bool left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (left && !orbiting && !ui.isOverPanel((float)mx, (float)my))
      orbiting = true;
    if (!left)
      orbiting = false;
    if (orbiting) {
      yaw += (float)(mx - lastX) * 0.006f;
      pitch = clamp(pitch + (float)(my - lastY) * 0.006f, -1.5f, 1.5f);
    }
    lastX = mx;
    lastY = my;
    if (!ui.isOverPanel((float)mx, (float)my))
      distance = clamp(distance * std::pow(0.9f, scrollSum), 1.0f, 3000.0f);
    scrollSum = 0.0f;

    // Moving the point it orbits
    auto held = [&](int key) { return glfwGetKey(window, key) == GLFW_PRESS ? 1.0f : 0.0f; };
    vec3 ahead(std::sin(yaw), 0.0f, -std::cos(yaw)), side(std::cos(yaw), 0.0f, std::sin(yaw));
    float speed = 0.5f * distance + 5.0f;
    focus += (ahead * (held(GLFW_KEY_W) - held(GLFW_KEY_S)) + side * (held(GLFW_KEY_D) - held(GLFW_KEY_A)) +
              vec3(0.0f, held(GLFW_KEY_E) - held(GLFW_KEY_Q), 0.0f)) * speed * dt;

    // The reference cube sits on its base (its model is centred on its origin)
    stage.relocate(*cube, cubeAt + vec3(0.0f, CUBE_SIDE * 0.5f, 0.0f));
    stage.update(dt);

    vec3 forward(std::sin(yaw) * std::cos(pitch), -std::sin(pitch), -std::cos(yaw) * std::cos(pitch));
    vec3 eye = focus - forward * distance;
    camera.setAngles(yaw, pitch);
    camera.reposition(eye.x, eye.y, eye.z);
    camera.update();

    char hint[160];
    snprintf(hint, sizeof(hint), "caja %.0f x %.0f x %.0f m | %d muestras por rayo | WASD/QE: mover | rueda: zoom",
             size.x, size.y, size.z, CloudRenderer::SAMPLES);
    ui.setHint(hint);
    ui.update();

    const Environment &env = stage.getEnvironment();
    shader.use();
    light.setColor(env.lightColor.r, env.lightColor.g, env.lightColor.b);
    light.moveTo(env.lightDir.x * 100.0f, env.lightDir.y * 100.0f, env.lightDir.z * 100.0f);
    shader.setVector3("moonDir", env.lightDir.x, env.lightDir.y, env.lightDir.z);
    shader.setVector3("fogColor", env.horizon.r, env.horizon.g, env.horizon.b);
    shader.setVector3("skyZenith", env.skyZenith.r, env.skyZenith.g, env.skyZenith.b);
    shader.setVector3("sunDir", env.sunDir.x, env.sunDir.y, env.sunDir.z);
    shader.setFloat("starAlpha", env.starAlpha);
    shader.setInt("spotCount", 0);
    shader.setFloat("time", (float)now);
    glClearColor(env.horizon.x, env.horizon.y, env.horizon.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    stage.render(&shader, camera.getPosition(), now);

    vec3 half = size * 0.5f;
    clouds.draw({CloudBox{centre - half, centre + half}}, camera.getPosition(), camera.getViewProjection(),
                env.lightColor, sunHeight, (float)now);
    ui.draw();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  glfwTerminate();
  return 0;
}
