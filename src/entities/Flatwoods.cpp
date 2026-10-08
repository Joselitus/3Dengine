#include "Flatwoods.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>


using namespace glm;
using namespace std;

namespace {
const float PI = 3.14159265f;
const float TURN_RATE = 1.5f;   // rad/s, as it turns to face the player
const float CLIMB_RATE = 1.5f;  // m/s, as it rises or sinks to the height it wants
const float BOB_HEIGHT = 0.08f, BOB_RATE = 1.3f; // its floating up and down (m, rad/s)
const float NEAR = 6.0f;        // m: this near the player it rises to his floor (in the RV)
// The middle of its body (for the flashlight)
const float MIDDLE = 1.4f;

float angleTo(float a, float b) {
  float d = std::fmod(b - a + PI, 2.0f * PI);
  if (d < 0.0f)
    d += 2.0f * PI;
  return d - PI;
}
} // namespace

Flatwoods::Flatwoods(shared_ptr<Model> body, shared_ptr<Model> eyes)
    : DynamicGameObject(body, make_shared<Capsule>(0.5f, 2.8f)), random(random_device()()) {
  addPart(eyes, 2); // (red, glowing)
  setGravity(0.0f);
  setCollidable(false); // (it goes through anything)
  setVisible(false);
}

void Flatwoods::enter(State next) {
  state = next;
  stateTime = 0.0f;
}

// It comes into being APPEAR_DISTANCE from the RV: behind it, or on a new side
void Flatwoods::appear(bool fromBehind) {
  vec3 centre(0.0f), forward(0.0f, 0.0f, 1.0f);
  if (vehicle)
    vehicle(centre, forward);
  if (fromBehind) {
    // (a smaller angle about +y turns "behind" towards the RV's +x, the driver's side)
    side = std::atan2(-forward.x, -forward.z) - BEHIND_SKEW;
  } else {
    float last = side;
    do
      side = std::uniform_real_distribution<float>(-PI, PI)(random);
    while (std::fabs(angleTo(last, side)) < MIN_TURN);
  }
  position = centre + vec3(std::sin(side), 0.0f, std::cos(side)) * APPEAR_DISTANCE;
  float ground = centre.y;
  if (floorHeight)
    floorHeight(position.x, position.z, ground);
  position.y = ground + HOVER;
  velocity = vec3(0.0f);
  cameTonight = true;
  enter(State::Waiting);
}

void Flatwoods::vanish() { enter(State::Away); }

void Flatwoods::release() {
  if (state == State::Holding)
    vanish();
}

void Flatwoods::setMirrorView(bool inMirror) {
  mirrorView = inMirror;
  setVisible(mirrorView && (state == State::Waiting || state == State::Coming));
}

void Flatwoods::update(double dt) {
  GameObject::update(dt);
  float dtf = (float)dt;
  stateTime += dtf;
  bool night = isNight ? isNight() : true;
  if (!night) { // at dawn it goes; the next night it starts again
    if (state != State::Away)
      vanish();
    cameTonight = false;
    setMirrorView(mirrorView);
    return;
  }
  vec3 player = target ? target() : position;
  switch (state) {
  case State::Away:
    if (!cameTonight)
      appear(true);
    else if (stateTime >= COME_BACK_TIME)
      appear(false);
    break;
  case State::Waiting:
  case State::Coming: {
    if (lit && lit(position + vec3(0.0f, MIDDLE, 0.0f))) { // the flashlight: it is gone
      vanish();
      break;
    }
    bool coming = playerInRV && playerInRV();
    state = coming ? State::Coming : State::Waiting;
    vec2 to(player.x - position.x, player.z - position.z);
    float distance = length(to);
    if (coming) {
      if (distance <= CATCH_DISTANCE) {
        enter(State::Holding);
        if (possess)
          possess();
        break;
      }
      float step = std::min(FLOAT_SPEED * dtf, distance);
      position.x += to.x / distance * step;
      position.z += to.y / distance * step;
    }
    // it floats over the floor (or rises to his, in the RV, once it is near), bobbing a little
    float ground = position.y - HOVER;
    if (floorHeight)
      floorHeight(position.x, position.z, ground);
    if (coming && distance < NEAR)
      ground = std::max(ground, player.y);
    phase += BOB_RATE * dtf;
    float wanted = ground + HOVER + BOB_HEIGHT * std::sin(phase);
    position.y += clamp(wanted - position.y, -CLIMB_RATE * dtf, CLIMB_RATE * dtf);
    // it turns to face him
    if (distance > 0.01f)
      yaw += clamp(angleTo(yaw, std::atan2(to.x, to.y)), -TURN_RATE * dtf, TURN_RATE * dtf);
    break;
  }
  case State::Holding:
    break; // (it is in him: the map walks his body out)
  }
  velocity = vec3(0.0f);
  // a slow sway as it floats
  rotation = rotate(mat4(1.0f), yaw, vec3(0.0f, 1.0f, 0.0f)) *
             rotate(mat4(1.0f), 0.03f * std::sin(phase * 0.7f), vec3(0.0f, 0.0f, 1.0f));
  setMirrorView(mirrorView);
}

void Flatwoods::describe(vector<string> &lines) const {
  DynamicGameObject::describe(lines);
  const char *names[] = {"no esta", "espera", "viene a por ti", "te controla"};
  lines.push_back(string("Flatwoods: ") + names[(int)state] + (cameTonight ? "" : " (aun no ha venido esta noche)"));
}
