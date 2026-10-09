// The creature testing tool: a creature on an empty floor (nothing else) to try out its
// animations, above all the procedural ones (SpiderGait, Ragdoll). Pick the creature and the
// animation in the menu (Tab), or from the command line:
//   ../test/creature_testing [--creature NAME] [--animation NAME|NUMBER] [--list] [--ray]
// Left button on the creature: drag it (a ragdoll is carried up, and thrown when it is let
// go); left button elsewhere: orbit the camera; wheel: zoom; right click on the floor: it
// walks there. Keys: Tab (menu), C (stop), R (start again in the middle), F (the camera
// follows it), K (draw the skeleton of the animation, red, over the creature), Esc (quit).
// The creatures and their animations are in CreatureEntry lists (see TestAnimation.h); to
// test another creature, add its entry to creatureEntries().
#include <GL/glew.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <unistd.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Camera.h"
#include "CreatureSelector.h"
#include "GameStage.h"
#include "Light.h"
#include "LineRenderer.h"
#include "MaterialMap.h"
#include "Shader.h"
#include "TestAnimation.h"
#include "UIManager.h"

using namespace std;
using namespace glm;

namespace {

const int WIDTH = 1000, HEIGHT = 700;
const float PICK_RADIUS = 0.30f;    // how close to a joint the cursor must be to take the creature
const float TURN_RATE = 10.0f;      // 1/s: how fast it turns to face where it is taken or goes
const float ARRIVE_DISTANCE = 0.3f; // how close it gets to the point it was sent to

// The empty space: a flat floor and a bit of light
class ToolStage : public GameStage {
public:
  explicit ToolStage(FloorMode mode) : GameStage(mode) {
    environment.lightDir = normalize(vec3(0.4f, 0.8f, 0.5f));
    environment.horizon = vec3(0.62f, 0.66f, 0.72f);
    const char *floorFile = "../tools/creature_testing/floor/floor.obj";
    auto ground = make_shared<GameObject>(loadModel(floorFile));
    ground->setCollidable(false); // it is the floor, not an obstacle
    add(ground);
    setFloor(loadModel(floorFile), vec3(0.0f), MaterialMap::uniform(FloorMaterial::Sand));
  }
};

// A ray from the camera through a point of the window (in window pixels)
void cursorRay(GLFWwindow *window, Camera &camera, double x, double y, vec3 &origin, vec3 &dir) {
  int w, h;
  glfwGetWindowSize(window, &w, &h);
  float nx = 2.0f * (float)x / w - 1.0f, ny = 1.0f - 2.0f * (float)y / h;
  mat4 inv = inverse(camera.getViewProjection());
  vec4 a = inv * vec4(nx, ny, -1.0f, 1.0f), b = inv * vec4(nx, ny, 1.0f, 1.0f);
  origin = vec3(a) / a.w;
  dir = normalize(vec3(b) / b.w - origin);
}

// Where the ray meets the horizontal plane at `height` (false if it doesn't, in front of it)
bool rayPlane(const vec3 &origin, const vec3 &dir, float height, vec3 &hit) {
  if (std::fabs(dir.y) < 1e-5f)
    return false;
  float t = (height - origin.y) / dir.y;
  if (t < 0.0f)
    return false;
  hit = origin + dir * t;
  return true;
}

float distanceToRay(const vec3 &origin, const vec3 &dir, const vec3 &p) {
  vec3 d = p - origin;
  return length(d - dir * std::max(dot(d, dir), 0.0f));
}

float scrollSum = 0.0f; // the wheel, collected by its callback
void onScroll(GLFWwindow *, double, double dy) { scrollSum += (float)dy; }

// Shaders and assets are loaded with paths relative to src/; the binary is built into test/
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
#if defined(GLFW_PLATFORM) && !defined(__APPLE__) // macOS has no X11
  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return nullptr;
  }
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "creature testing", NULL, NULL);
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

string lower(string s) {
  for (char &c : s)
    c = (char)tolower(c);
  return s;
}

} // namespace

int main(int argc, char **argv) {
  FloorMode floorMode = FloorMode::HeightField;
  bool list = false;
  string creatureArg, animationArg;
  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    if (arg == "--ray")
      floorMode = FloorMode::DownwardRay;
    else if (arg == "--list")
      list = true;
    else if (arg == "--creature" && i + 1 < argc)
      creatureArg = argv[++i];
    else if (arg == "--animation" && i + 1 < argc)
      animationArg = argv[++i];
    else {
      fprintf(stderr, "Unknown option '%s'\n", argv[i]);
      return 1;
    }
  }

  const vector<CreatureEntry> entries = creatureEntries();
  if (list) {
    for (const CreatureEntry &e : entries) {
      printf("%s\n", e.name.c_str());
      for (size_t a = 0; a < e.animations.size(); a++)
        printf("  %zu  %s\n", a, e.animations[a].c_str());
    }
    return 0;
  }
  // The creature and the animation to start with
  int creatureIndex = 0, animationIndex = 0;
  if (!creatureArg.empty()) {
    creatureIndex = -1;
    for (size_t i = 0; i < entries.size(); i++)
      if (lower(entries[i].name).find(lower(creatureArg)) != string::npos) {
        creatureIndex = (int)i;
        break;
      }
    if (creatureIndex < 0) {
      fprintf(stderr, "No creature called '%s' (see --list)\n", creatureArg.c_str());
      return 1;
    }
  }
  if (!animationArg.empty()) {
    const vector<string> &names = entries[creatureIndex].animations;
    animationIndex = -1;
    char *end = nullptr;
    long number = strtol(animationArg.c_str(), &end, 10);
    if (*end == '\0' && number >= 0 && number < (long)names.size())
      animationIndex = (int)number;
    for (size_t i = 0; animationIndex < 0 && i < names.size(); i++)
      if (lower(names[i]).find(lower(animationArg)) != string::npos)
        animationIndex = (int)i;
    if (animationIndex < 0) {
      fprintf(stderr, "'%s' has no animation '%s' (see --list)\n", entries[creatureIndex].name.c_str(),
              animationArg.c_str());
      return 1;
    }
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
  LineRenderer lines;
  bool showSkeleton = false;

  // The stage and the animation being tested (rebuilt when another one is chosen)
  unique_ptr<ToolStage> stage;
  unique_ptr<TestAnimation> animation;
  TestBody body;
  bool hasDestination = false;
  vec3 destination(0.0f);
  int requestedCreature = creatureIndex, requestedAnimation = animationIndex; // chosen, not yet built
  bool restartRequested = false;
  bool follow = false;

  auto build = [&]() {
    creatureIndex = requestedCreature;
    animationIndex = requestedAnimation;
    animation.reset(); // (before the stage that holds its object)
    stage.reset(new ToolStage(floorMode));
    animation = entries[creatureIndex].create(animationIndex, *stage);
    body = TestBody();
    hasDestination = false;
    animation->reset(body);
  };
  build();

  ui.bindKey(GLFW_KEY_ESCAPE, [&]() { glfwSetWindowShouldClose(window, true); });
  ui.bindKey(GLFW_KEY_TAB, [&]() {
    ui.open(new CreatureSelector(entries, creatureIndex, animationIndex, GLFW_KEY_TAB,
                                 [&](int c, int a) {
                                   requestedCreature = c;
                                   requestedAnimation = a;
                                   restartRequested = true;
                                 }));
  });
  ui.bindKey(GLFW_KEY_R, [&]() { restartRequested = true; });
  ui.bindKey(GLFW_KEY_C, [&]() { hasDestination = false; });
  ui.bindKey(GLFW_KEY_F, [&]() { follow = !follow; });
  ui.bindKey(GLFW_KEY_K, [&]() { showSkeleton = !showSkeleton; });

  // The camera orbits a point: yaw (positive turns right) and pitch (positive looks down)
  float yaw = 0.6f, pitch = 0.45f, distance = 7.0f;
  vec3 focus(0.0f, 0.6f, 0.0f);
  bool dragging = false, orbiting = false, rightDown = false;
  vec3 grabOffset(0.0f);
  double lastX = 0.0, lastY = 0.0, rightX = 0.0, rightY = 0.0;
  vec3 lastPosition = body.position;

  double lastTime = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    double now = glfwGetTime();
    double dt = std::min(now - lastTime, 0.1);
    lastTime = now;
    camera.resize();

    if (restartRequested) {
      restartRequested = false;
      dragging = false;
      build();
      lastPosition = body.position;
    }

    // The mouse (not while a menu is open)
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    bool panels = ui.hasPanels();
    bool left = !panels && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool right = !panels && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    vec3 origin, dir;
    cursorRay(window, camera, mx, my, origin, dir);
    if (left && !dragging && !orbiting) {
      vector<vec3> joints;
      animation->joints(joints);
      bool picked = false;
      for (const vec3 &p : joints)
        picked = picked || distanceToRay(origin, dir, p) < PICK_RADIUS;
      vec3 hit;
      if (picked && rayPlane(origin, dir, body.position.y, hit)) {
        dragging = true;
        grabOffset = body.position - hit;
        hasDestination = false;
      } else {
        orbiting = true;
      }
    }
    if (!left) {
      dragging = false;
      orbiting = false;
    }
    if (orbiting) {
      yaw += (float)(mx - lastX) * 0.006f;
      pitch = glm::clamp(pitch + (float)(my - lastY) * 0.006f, 0.05f, 1.5f);
    }
    body.held = dragging;
    if (dragging) {
      vec3 hit;
      if (rayPlane(origin, dir, body.position.y, hit)) {
        vec3 to = hit + grabOffset;
        vec3 moved(to.x - body.position.x, 0.0f, to.z - body.position.z);
        float height = 0.0f;
        stage->floorAt(to.x, to.z, height);
        body.position = vec3(to.x, height, to.z);
        if (length(moved) > 0.004f) { // it faces where it is taken, turning smoothly
          float wanted = atan2(moved.x, moved.z);
          body.heading += atan2(sin(wanted - body.heading), cos(wanted - body.heading)) *
                          std::min(1.0f, (float)dt * TURN_RATE);
        }
      }
    }
    // A right click (press and release without moving) sends it to that point
    if (right && !rightDown) {
      rightDown = true;
      rightX = mx;
      rightY = my;
    }
    if (!right && rightDown) {
      rightDown = false;
      vec3 hit;
      if (animation->walks() && std::fabs(mx - rightX) < 4.0 && std::fabs(my - rightY) < 4.0 &&
          !dragging && rayPlane(origin, dir, 0.0f, hit)) {
        hasDestination = true;
        destination = vec3(hit.x, 0.0f, hit.z);
      }
    }
    lastX = mx;
    lastY = my;
    distance = glm::clamp(distance * std::pow(0.9f, scrollSum), 2.0f, 40.0f);
    scrollSum = 0.0f;

    // Walking to the point it was sent to
    if (hasDestination && !dragging) {
      vec3 d = destination - body.position;
      d.y = 0.0f;
      float left2 = length(d);
      if (left2 < ARRIVE_DISTANCE) {
        hasDestination = false;
      } else {
        float step = std::min(left2, entries[creatureIndex].walkSpeed * (float)dt);
        body.position += d / left2 * step;
        float wanted = atan2(d.x, d.z);
        body.heading += atan2(sin(wanted - body.heading), cos(wanted - body.heading)) *
                        std::min(1.0f, (float)dt * TURN_RATE);
        float height = 0.0f;
        stage->floorAt(body.position.x, body.position.z, height);
        body.position.y = height;
      }
    }

    body.viewer = camera.getPosition(); // (where it was the last frame)
    stage->update(dt);
    animation->update(dt, body);
    // How fast the body really moves (whatever moved it): what the animations are told
    if (dt > 0.0) {
      vec3 moved = (body.position - lastPosition) / (float)dt;
      moved.y = 0.0f;
      body.velocity += (moved - body.velocity) * std::min(1.0f, (float)dt * 15.0f);
    }
    lastPosition = body.position;

    // The camera
    if (follow) {
      vec3 target = body.position + vec3(0.0f, 0.6f, 0.0f);
      focus += (target - focus) * std::min(1.0f, (float)dt * 4.0f);
    }
    vec3 forward(std::sin(yaw) * std::cos(pitch), -std::sin(pitch), -std::cos(yaw) * std::cos(pitch));
    vec3 eye = focus - forward * distance;
    camera.setAngles(yaw, pitch);
    camera.reposition(eye.x, eye.y, eye.z);
    camera.update();

    char hint[200];
    snprintf(hint, sizeof(hint), "%s | %s | %.1f m/s | %s | Tab: menu",
             entries[creatureIndex].name.c_str(), animation->name().c_str(), length(body.velocity),
             animation->status().c_str());
    ui.setHint(hint);
    ui.update();

    const Environment &env = stage->getEnvironment();
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
    stage->render(&shader, camera.getPosition(), now);
    if (showSkeleton) { // the skeleton of the animation over the creature
      vector<pair<vec3, vec3>> bones;
      animation->skeleton(bones);
      for (const auto &b : bones)
        lines.line(b.first, b.second, vec4(1.0f, 0.2f, 0.2f, 1.0f));
      lines.draw(camera);
    }
    ui.draw();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  animation.reset();
  glfwTerminate();
  return 0;
}
