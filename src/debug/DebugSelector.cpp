#include "DebugSelector.h"

#include <cxxabi.h>
#include <cstdlib>
#include <typeinfo>

#include "Camera.h"
#include "GameStage.h"
#include "Interactable.h"
#include "PropertyPanel.h"
#include "TextFormat.h"

using namespace std;
using namespace glm;

#define MAX_DISTANCE 300.0f // the camera's far plane
#define FLOOR_STEP 0.25f    // metres between the ray's floor tests
#define VELOCITY_TIME 0.5f  // the velocity is drawn as 0.5 s of travel
#define BOX_MARGIN 16.0f    // from the corner of the screen
#define CROSSHAIR 8.0f      // half the size of the crosshair, pixels
#define SNAP_STEP 15.0f     // degrees between the headings Shift snaps to

namespace {
const vec4 SHAPE_COLOR(1.0f, 0.55f, 0.15f, 1.0f);
const vec4 BOUNDS_COLOR(0.35f, 0.7f, 1.0f, 0.8f);
const vec4 VELOCITY_COLOR(0.3f, 1.0f, 0.35f, 1.0f);
const vec4 TITLE_COLOR(1.0f, 0.75f, 0.35f, 1.0f);
const vec4 TARGET_COLOR(1.0f, 1.0f, 1.0f, 0.9f);
// The floor materials, in Spanish
const char *materialName(FloorMaterial material) {
  switch (material) {
  case FloorMaterial::Sand: return "arena";
  case FloorMaterial::Asphalt: return "asfalto";
  case FloorMaterial::Grass: return "hierba";
  case FloorMaterial::Count: break;
  }
  return "?";
}
} // namespace

DebugSelector::DebugSelector(GLFWwindow *window, UIManager &ui,
                             const Controls &controls)
    : window(window), ui(ui), controls(controls) {}

DebugSelector::~DebugSelector() {
  if (mode != Mode::Off)
    ui.removeOverlay(this);
}

void DebugSelector::setMode(Mode next) {
  if (next != Mode::Off && mode == Mode::Off)
    ui.addOverlay(this);
  else if (next == Mode::Off && mode != Mode::Off)
    ui.removeOverlay(this);
  mode = next;
  hasTarget = false;
  turning = false;
  hovered.reset();
  hoveredName.clear();
}

bool DebugSelector::capturesMouse() const {
  return mode == Mode::Place && !selected.expired() &&
         glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
}

void DebugSelector::clear() {
  selected.reset();
  selectedName.clear();
  hovered.reset();
  hoveredName.clear();
  info.clear();
}

string DebugSelector::nameOf(const GameObject &object) {
  const char *mangled = typeid(object).name();
  int status = 0;
  char *demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
  string name = status == 0 && demangled ? demangled : mangled;
  free(demangled);
  if (const Interactable *interactable =
          dynamic_cast<const Interactable *>(&object))
    name += " '" + interactable->getInteractionName() + "'";
  return name;
}

shared_ptr<GameObject> DebugSelector::pick(const GameStage &stage,
                                           const vec3 &origin,
                                           const vec3 &direction) const {
  shared_ptr<GameObject> best;
  float bestDistance = MAX_DISTANCE;
  auto consider = [&](const shared_ptr<GameObject> &object) {
    // Not what can't be seen (a first-person player) or isn't solid (the
    // floor, the road)
    if (!object->isVisible() || !object->isCollidable())
      return;
    float distance;
    if (object->getShape().raycast(object->getPose(), origin, direction,
                                   distance) &&
        distance < bestDistance) {
      best = object;
      bestDistance = distance;
    }
  };
  for (const auto &object : stage.getObjects())
    consider(object);
  for (const auto &object : stage.getDynamicObjects())
    consider(object);
  if (!best)
    return nullptr;
  // Hidden behind a dune: the ray goes under the floor before reaching it
  vec3 floorPoint;
  if (floorHit(stage, origin, direction, bestDistance, floorPoint))
    return nullptr;
  return best;
}

bool DebugSelector::floorHit(const GameStage &stage, const vec3 &origin,
                             const vec3 &direction, float maxDistance,
                             vec3 &point) {
  auto below = [&](float t) {
    vec3 p = origin + direction * t;
    float height;
    return stage.floorAt(p.x, p.z, height) && p.y < height;
  };
  float before = 0.0f;
  for (float t = FLOOR_STEP; t < maxDistance; t += FLOOR_STEP) {
    if (below(t)) {
      // Between the last step above it and this one: halve it down to
      // millimetres
      float after = t;
      for (int i = 0; i < 10; i++) {
        float middle = (before + after) * 0.5f;
        if (below(middle))
          after = middle;
        else
          before = middle;
      }
      point = origin + direction * after;
      float height;
      if (stage.floorAt(point.x, point.z, height))
        point.y = height;
      return true;
    }
    before = t;
  }
  return false;
}

vec3 DebugSelector::destination(const GameStage &stage,
                                const GameObject &object,
                                const vec3 &floorPoint) const {
  // As high above the floor as it is now (props sunk a little stay sunk, the
  // head of a satellite stays on top of its post)
  vec3 now = object.getPosition();
  float height, above = 0.0f;
  if (stage.floorAt(now.x, now.z, height))
    above = now.y - height;
  return floorPoint + vec3(0.0f, above, 0.0f);
}

void DebugSelector::select(const GameStage &stage,
                           shared_ptr<GameObject> object) {
  selected = object;
  selectedName = object ? labelOf(stage, *object) : "";
}

string DebugSelector::labelOf(const GameStage &stage, const GameObject &object) {
  string label = nameOf(object);
  int index = 0;
  for (const auto &o : stage.getObjects()) {
    if (o.get() == &object)
      label += textFormat(" #%d", index);
    index++;
  }
  index = 0;
  for (const auto &o : stage.getDynamicObjects()) {
    if (o.get() == &object)
      label += textFormat(" #d%d", index);
    index++;
  }
  return label;
}

void DebugSelector::edit(const GameStage &stage, shared_ptr<GameObject> object) {
  if (!object)
    return;
  placeAtRight(ui.open(new PropertyPanel(labelOf(stage, *object), object)));
}

void DebugSelector::editWorld(GameStage &stage) {
  vector<Property> properties;
  stage.getProperties(properties);
  placeAtRight(ui.open(new PropertyPanel("Mundo", properties)));
}

void DebugSelector::placeAtRight(UIPanel *panel) {
  int width, height;
  glfwGetWindowSize(window, &width, &height);
  panel->moveTo(width - panel->getWidth() - BOX_MARGIN,
                std::max(BOX_MARGIN, (height - panel->preferredHeight()) / 2));
}

void DebugSelector::refresh(GameStage &stage, Camera &camera) {
  info.clear();
  shared_ptr<GameObject> object = selected.lock();
  if (!object && mode != Mode::Inspect) {
    info.push_back(mode == Mode::Place
                       ? "Ningun objeto elegido: eligelo antes con " +
                             controls.keyName(Action::DebugSelect)
                       : "Apunta a un objeto y haz clic");
    return;
  }
  if (mode == Mode::Place) {
    info.push_back(hasTarget ? "Destino: " + textOf(target)
                             : "Destino: apunta al suelo");
    info.push_back("Ahora: " + textOf(object->getPosition()));
    info.push_back(textFormat("Rumbo: %.1f grados%s",
                              tidy(degrees(object->getHeading())),
                              !turning   ? ""
                              : snapping ? "  (girando de 15 en 15)"
                                         : "  (girando)"));
    return;
  }
  if (mode == Mode::Inspect) {
    shared_ptr<GameObject> under = hovered.lock();
    vector<Property> properties;
    if (under)
      under->getProperties(properties);
    else
      stage.getProperties(properties); // no object: the world
    for (const Property &p : properties)
      if (p.kind != Property::Kind::Action)
        info.push_back(p.name + ": " + p.valueText());
    if (properties.empty())
      info.push_back("(sin propiedades)");
    return;
  }
  object->describe(info);
  // The floor under it, as the stage sees it
  vec3 p = object->getPosition();
  float height;
  if (stage.floorAt(p.x, p.z, height))
    info.push_back(textFormat("Suelo: altura %.2f (%.2f por debajo), %s",
                              height, p.y - height,
                              materialName(stage.materialAt(p.x, p.z))));
  else
    info.push_back("Suelo: fuera del suelo");
  info.push_back(textFormat("Distancia a la camara: %.1f m",
                            length(p - camera.getPosition())));
}

void DebugSelector::update(GameStage &stage, Camera &camera, bool canPick) {
  bool left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  bool right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
  // Only new presses, so the click that closed a panel doesn't select
  bool leftPressed = left && !leftWasDown, rightPressed = right && !rightWasDown;
  leftWasDown = left;
  rightWasDown = right;
  if (mode == Mode::Off)
    return;
  vec3 eye = camera.getPosition(), forward = camera.getForward();
  shared_ptr<GameObject> object = selected.lock();
  if (mode == Mode::Inspect) {
    // What the crosshair points at; it stays while a panel is open
    if (canPick) {
      shared_ptr<GameObject> under = pick(stage, eye, forward);
      hovered = under;
      hoveredName = under ? labelOf(stage, *under) : "Mundo";
      if (leftPressed && under)
        edit(stage, under);
      else if (leftPressed)
        editWorld(stage);
      else if (rightPressed)
        edit(stage, stage.getPlayer());
    }
  } else if (mode == Mode::Select) {
    if (canPick && leftPressed)
      select(stage, pick(stage, eye, forward));
    else if (canPick && rightPressed)
      select(stage, stage.getPlayer());
  } else {
    vec3 floorPoint;
    hasTarget = object && floorHit(stage, eye, forward, MAX_DISTANCE, floorPoint);
    if (hasTarget)
      target = destination(stage, *object, floorPoint);
    if (canPick && leftPressed && hasTarget) {
      stage.relocate(*object, target);
      hasTarget = false; // it is there now: no outline until the next frame
    }
    // Right button held: the mouse turns the object (the camera doesn't, see
    // capturesMouse). Moving right turns it clockwise seen from above, as the
    // camera would turn. With Shift, the heading snaps to SNAP_STEP degrees
    // (to the multiples of it, not steps from where it was).
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    snapping = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
               glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    if (object && canPick && right) {
      if (!turning)
        turnHeading = object->getHeading();
      turnHeading -= camera.getSensitivity() * (float)(x - lastCursorX) *
                     (turning ? 1.0f : 0.0f);
      float wanted = turnHeading;
      if (snapping) {
        float step = radians(SNAP_STEP);
        wanted = std::round(turnHeading / step) * step;
      }
      // The shortest way there
      float change = std::remainder(wanted - object->getHeading(),
                                    2.0f * pi<float>());
      if (std::fabs(change) > 1e-5f)
        stage.turn(*object, change);
      turning = true;
      lastCursorX = x;
    } else {
      turning = false;
    }
  }
  refresh(stage, camera);
}

void DebugSelector::outline(const CollisionShape &shape, const Pose &pose,
                            const vec4 &color) {
  if (const Capsule *capsule = dynamic_cast<const Capsule *>(&shape)) {
    vec3 a, b;
    float r;
    capsule->segment(pose, a, b, r);
    vec3 axis = length(b - a) > 1e-5f ? normalize(b - a) : vec3(0, 1, 0);
    vec3 u = normalize(cross(axis, std::fabs(axis.y) < 0.9f ? vec3(0, 1, 0)
                                                            : vec3(1, 0, 0)));
    vec3 v = cross(axis, u);
    lines.circle(a, axis, r, color);
    lines.circle(b, axis, r, color);
    for (const vec3 &side : {u, -u, v, -v})
      lines.line(a + side * r, b + side * r, color);
    lines.arc(b, u, axis, r, color);
    lines.arc(b, v, axis, r, color);
    lines.arc(a, u, -axis, r, color);
    lines.arc(a, v, -axis, r, color);
  } else if (const Box *box = dynamic_cast<const Box *>(&shape)) {
    vec3 c, h;
    box->world(pose, c, h);
    lines.box(c, pose.rotation, h, color);
  } else if (const CompoundShape *compound = dynamic_cast<const CompoundShape *>(&shape)) {
    for (const CompoundShape::Part &part : compound->getParts()) {
      if (!part.enabled)
        continue;
      Pose partPose = compound->partPose(part, pose);
      vec3 c, h;
      part.box.world(partPose, c, h);
      lines.box(c, partPose.rotation, h, color);
    }
  }
}

void DebugSelector::draw(Camera &camera) {
  shared_ptr<GameObject> object = mode == Mode::Inspect ? hovered.lock()
                                  : mode != Mode::Off      ? selected.lock()
                                                           : nullptr;
  if (!object)
    return;
  Pose pose = object->getPose();
  const CollisionShape &shape = object->getShape();
  outline(shape, pose, SHAPE_COLOR);
  vec3 min, max;
  shape.bounds(pose, min, max);
  lines.box(min, max, BOUNDS_COLOR);
  if (const DynamicGameObject *dynamic =
          dynamic_cast<const DynamicGameObject *>(object.get())) {
    vec3 centre = (min + max) * 0.5f;
    lines.line(centre, centre + dynamic->getVelocity() * VELOCITY_TIME,
               VELOCITY_COLOR);
  }
  // Where it would go: its outline there, and a line from here to there
  if (mode == Mode::Place && hasTarget) {
    Pose there = pose;
    there.position = target;
    outline(shape, there, TARGET_COLOR);
    lines.line(pose.position, target, TARGET_COLOR);
  }
  lines.draw(camera);
}

void DebugSelector::draw(UIRenderer &renderer, float width,
                         float height) const {
  // Crosshair: a cross with a dark outline, so it shows on sand and on sky
  float cx = width / 2, cy = height / 2;
  const vec4 dark(0.0f, 0.0f, 0.0f, 0.6f), light(1.0f, 1.0f, 1.0f, 0.95f);
  renderer.rect(cx - CROSSHAIR - 1, cy - 2, 2 * CROSSHAIR + 2, 4, dark);
  renderer.rect(cx - 2, cy - CROSSHAIR - 1, 4, 2 * CROSSHAIR + 2, dark);
  renderer.rect(cx - CROSSHAIR, cy - 1, 2 * CROSSHAIR, 2, light);
  renderer.rect(cx - 1, cy - CROSSHAIR, 2, 2 * CROSSHAIR, light);

  // The box of data, at the top left; not while a properties window is open
  // (it says the same, and they would overlap)
  if (mode == Mode::Inspect && ui.hasPanels())
    return;
  vector<string> text;
  const string &name = mode == Mode::Inspect ? hoveredName : selectedName;
  if (mode == Mode::Inspect) {
    text.push_back("MODO PROPIEDADES (" + controls.keyName(Action::DebugInspect) +
                   ": salir)");
    text.push_back("Clic izq.: cambiarlas   Clic der.: las del jugador");
  } else if (mode == Mode::Place) {
    text.push_back("MODO COLOCACION (" + controls.keyName(Action::DebugPlace) +
                   ": salir)");
    text.push_back("Clic izq.: llevarlo a la cruz   Clic der. + raton: girar");
    text.push_back("(+ Mayus: de 15 en 15 grados)");
  } else {
    text.push_back("MODO SELECCION (" + controls.keyName(Action::DebugSelect) +
                   ": salir, " + controls.keyName(Action::DebugPlace) +
                   ": colocar)");
    text.push_back("Clic izq.: objeto del centro   Clic der.: jugador");
    text.push_back(textFormat("FPS: %.0f", fps));
  }
  if (!name.empty())
    text.push_back(name);
  float boxWidth = 0.0f;
  for (const string &line : text)
    boxWidth = std::max(boxWidth, UIRenderer::textWidth(line));
  for (const string &line : info)
    boxWidth = std::max(boxWidth, UIRenderer::textWidth(line));
  float lineHeight = UIRenderer::textHeight() + 3.0f;
  float boxHeight = (text.size() + info.size()) * lineHeight +
                    (info.empty() ? 0.0f : UITheme::SPACING) +
                    2 * UITheme::PADDING;
  float x = BOX_MARGIN, y = BOX_MARGIN;
  boxWidth += 2 * UITheme::PADDING;
  renderer.rect(x, y, boxWidth, boxHeight, UITheme::PANEL);
  renderer.frame(x, y, boxWidth, boxHeight, 1.0f, UITheme::BORDER);
  float ty = y + UITheme::PADDING;
  for (size_t i = 0; i < text.size(); i++) {
    // The title, the help lines and, last, the selected object's name
    bool isName = !name.empty() && i + 1 == text.size();
    renderer.text(x + UITheme::PADDING, ty, text[i],
                  isName ? TITLE_COLOR : (i == 0 ? UITheme::TEXT : UITheme::MUTED));
    ty += lineHeight;
  }
  ty += UITheme::SPACING;
  for (const string &line : info) {
    renderer.text(x + UITheme::PADDING, ty, line, UITheme::TEXT);
    ty += lineHeight;
  }
}
