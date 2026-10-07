#include "Saucer.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "Bob.h"
#include "Camera.h"
#include "Stage.h"
#include "TextFormat.h"

using namespace glm;
using namespace std;

namespace {
const float PI = 3.14159265f;
// The ramp swings down until its foot touches the ground
const float RAMP_ANGLE = std::asin(Saucer::RAMP_HINGE_Y / Saucer::RAMP_LENGTH);
// Its body, for collisions: the hull, above the legs (one can't walk under it)
const vec3 HULL_HALF(Saucer::RADIUS * 0.72f, 0.65f, Saucer::RADIUS * 0.72f);
const vec3 HULL_CENTER(0.0f, Saucer::LEG_HEIGHT + 0.7f, 0.0f);
const vec3 BEAM_COLOR(0.55f, 0.85f, 1.0f);

float ease(float t) {
  t = clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
float angleTo(float a, float b) {
  float d = std::fmod(b - a + PI, 2.0f * PI);
  if (d < 0.0f)
    d += 2.0f * PI;
  return d - PI;
}
float approach(float value, float target, float step) {
  return value + clamp(target - value, -step, step);
}
} // namespace

Saucer::Saucer(shared_ptr<Model> hull, shared_ptr<Model> lights, shared_ptr<Model> legs,
               shared_ptr<Model> ramp, shared_ptr<Model> beam)
    : PlayableCharacter(hull, make_shared<Box>(HULL_HALF, HULL_CENTER)), random(random_device()()) {
  hullPart = 0;
  lightsPart = addPart(lights, 2); // glowing
  legsPart = addPart(legs);
  rampPart = addPart(ramp);
  beamPart = addPart(beam, 2);
  setGravity(0.0f);
  setMass(1e6f); // nothing pushes it
  setVisible(false);
  setCollidable(false);
}

void Saucer::setLanding(const vec3 &spot, float yaw) {
  landing = spot;
  landingYaw = yaw;
  position = spot + vec3(0.0f, HIGH_UP, 0.0f);
  place();
}

void Saucer::enter(Phase next) {
  phase = next;
  phaseTime = 0.0f;
  switch (next) {
  case Phase::Arriving: {
    // from far away over the trees, towards a point above where it lands
    float a = std::uniform_real_distribution<float>(0.0f, 2.0f * PI)(random);
    from = landing + vec3(std::cos(a) * FAR_AWAY, HIGH_UP, std::sin(a) * FAR_AWAY);
    to = landing + vec3(0.0f, HOVER_HEIGHT, 0.0f);
    position = from;
    setVisible(true);
    visitedTonight = true;
    taking = false;
    bobOut = false;
    rampOpen = 0.0f;
    legsOut = 0.0f;
    break;
  }
  case Phase::Descending:
    from = position;
    to = landing;
    spinFrom = spin;
    break;
  case Phase::Landed:
    setCollidable(true);
    onGround = true;
    break;
  case Phase::Ascending:
    setCollidable(false);
    from = position;
    to = position + vec3(0.0f, HOVER_HEIGHT, 0.0f);
    break;
  case Phase::Leaving: {
    float a = std::uniform_real_distribution<float>(0.0f, 2.0f * PI)(random);
    from = position;
    to = position + vec3(std::cos(a) * FAR_AWAY * 1.5f, HIGH_UP * 2.0f, std::sin(a) * FAR_AWAY * 1.5f);
    break;
  }
  case Phase::Away:
    setVisible(false);
    setCollidable(false);
    taking = false;
    break;
  default:
    break;
  }
}

void Saucer::boarded() {
  if (phase == Phase::Landed)
    enter(Phase::Closing);
}

// Bob is in it, coming out or going in, or near the foot of its ramp
bool Saucer::bobNearRamp() const {
  if (!bob)
    return false;
  Bob::Behavior b = bob->getBehavior();
  if (b == Bob::Behavior::Inside || b == Bob::Behavior::Exiting || b == Bob::Behavior::Boarding)
    return true;
  vec3 foot = rampFoot(), at = bob->getPosition();
  return length(vec2(at.x - foot.x, at.z - foot.z)) < RAMP_NEAR;
}

bool Saucer::isInteractionAvailable() const {
  // (with its ramp down, and Bob out of the way)
  return isRampDown() && bob && bob->isOut() && bob->getBehavior() != Bob::Behavior::Exiting &&
         bob->getBehavior() != Bob::Behavior::Boarding;
}

void Saucer::setPiloted(bool piloted) {
  if (piloted && phase == Phase::Landed) {
    enter(Phase::Piloted);
    engineOn = false;
    legsWanted = true;
    steer = vec2(0.0f);
    lift = 0.0f;
  } else if (!piloted && phase == Phase::Piloted) {
    engineOn = false;
    velocity = vec3(0.0f);
    enter(Phase::Landed);
  }
}

void Saucer::toggleEngine() {
  if (phase == Phase::Piloted)
    engineOn = !engineOn;
}

void Saucer::toggleLegs() {
  // (only in the air: on the ground it stands on them)
  if (phase == Phase::Piloted && !onGround)
    legsWanted = !legsWanted;
}

void Saucer::attachCamera(Camera *cam, float distance, float height) {
  camera = cam;
  if (camera)
    camera->attachTo(this, distance, height);
}

void Saucer::followCamera() {
  if (camera)
    camera->follow();
}

void Saucer::control(vec2 dir, float up, float cameraYaw) {
  // dir is in the camera's frame (x right, y backwards): turned by its heading, as Walker does
  steer = dir == vec2(0.0f) ? vec2(0.0f) : rotate(normalize(dir), cameraYaw);
  lift = up;
}

void Saucer::update(double dt) {
  GameObject::update(dt);
  float dtf = (float)dt;
  phaseTime += dtf;
  bool night = isNight ? isNight() : true;
  if (!night && phase == Phase::Away)
    visitedTonight = false; // (a new night will bring it again)
  switch (phase) {
  case Phase::Away:
    if (night && !visitedTonight)
      enter(Phase::Arriving);
    break;
  case Phase::Arriving: {
    // it glides in on a curve, slowing down as it gets there, spinning
    float t = ease(phaseTime / ARRIVE_TIME);
    position = mix(from, to, t) + vec3(0.0f, std::sin(t * PI) * 12.0f, 0.0f);
    spin += SPIN_RATE * dtf;
    if (phaseTime >= ARRIVE_TIME)
      enter(Phase::Descending);
    break;
  }
  case Phase::Descending: {
    float t = phaseTime / DESCEND_TIME;
    float k = 1.0f - (1.0f - t) * (1.0f - t); // fast at first, gentle at the end
    position = mix(from, to, clamp(k, 0.0f, 1.0f));
    // its turn slows and settles with the ramp where it must open
    spin = spinFrom + angleTo(spinFrom, landingYaw) * ease(t);
    legsOut = clamp((t - 0.4f) / 0.5f, 0.0f, 1.0f);
    if (phaseTime >= DESCEND_TIME) {
      position = landing;
      spin = landingYaw;
      legsOut = 1.0f;
      enter(Phase::Landed);
    }
    break;
  }
  case Phase::Landed: {
    // the ramp follows Bob: down while he is near, up when he walks away
    rampOpen = approach(rampOpen, bobNearRamp() ? 1.0f : 0.0f, dtf / RAMP_TIME);
    if (!bobOut && rampOpen >= 0.99f && bob) {
      bobOut = true;
      bob->disembark(); // out comes Bob
    }
    break;
  }
  case Phase::Piloted:
    rampOpen = approach(rampOpen, 0.0f, dtf / RAMP_TIME); // (it flies with the ramp up)
    legsOut = approach(legsOut, legsWanted ? 1.0f : 0.0f, dtf / LEGS_TIME);
    break; // (it moves in contactFloor)
  case Phase::Closing:
    rampOpen = approach(rampOpen, 0.0f, dtf / RAMP_TIME);
    if (rampOpen <= 0.0f)
      enter(Phase::Ascending);
    break;
  case Phase::Ascending: {
    float t = ease(phaseTime / ASCEND_TIME);
    position = mix(from, to, t);
    legsOut = std::min(legsOut, 1.0f - clamp(phaseTime / (ASCEND_TIME * 0.6f), 0.0f, 1.0f));
    spin += SPIN_RATE * dtf * t;
    if (phaseTime >= ASCEND_TIME)
      enter(Phase::Leaving);
    break;
  }
  case Phase::Leaving: {
    float t = phaseTime / LEAVE_TIME;
    position = mix(from, to, t * t); // faster and faster
    spin += SPIN_RATE * dtf;
    if (phaseTime >= LEAVE_TIME)
      enter(Phase::Away);
    break;
  }
  }
  if (phase != Phase::Piloted)
    velocity = vec3(0.0f);
}

// Flying it, with the stage's floor: across and up and down as the player wants (or sinking, with
// the engine off), never below the ground (on its legs, or on its belly without them), never
// above CEILING, never off the map
void Saucer::fly(const Stage &stage, double dt) {
  float dtf = (float)dt;
  vec3 want(0.0f, -SINK_SPEED, 0.0f);
  if (engineOn)
    want = vec3(steer.x * FLY_SPEED, lift * CLIMB_SPEED, steer.y * FLY_SPEED);
  velocity += (want - velocity) * std::min(1.0f, RESPONSE * dtf);
  position += velocity * dtf;
  float ground;
  if (!stage.floorAt(position.x, position.z, ground))
    ground = position.y - LEG_HEIGHT;
  // the lowest it can be: on its legs, or as they go in, down to its belly
  float lowest = ground - (LEG_HEIGHT - 0.12f) * (1.0f - legsOut);
  if (position.y <= lowest) {
    position.y = lowest;
    if (velocity.y < 0.0f)
      velocity.y = 0.0f;
  }
  onGround = position.y <= lowest + 0.05f;
  if (onGround) // (it does not slide on the ground)
    velocity.x = velocity.z = 0.0f;
  position.y = std::min(position.y, ground + CEILING);
  stage.keepInsideFloor(position, velocity, RADIUS);
}

bool Saucer::contactFloor(const Stage &stage, double dt) {
  if (phase == Phase::Piloted)
    fly(stage, dt);
  return true; // (on its own, it flies where its phase takes it)
}

void Saucer::applyCollision(const vec3 &push, const vec3 &velocityChange) {
  // nothing moves it, except a tree (or anything else) it is flown into
  if (phase == Phase::Piloted) {
    position += push;
    velocity += velocityChange;
  }
}

void Saucer::place() {
  rotation = rotate(mat4(1.0f), spin, vec3(0.0f, 1.0f, 0.0f));
  // the legs slide up into the hull; the ramp turns down about its hinge
  setPartVisible(legsPart, legsOut > 0.02f);
  setPartTransform(legsPart, glm::translate(mat4(1.0f), vec3(0.0f, (1.0f - legsOut) * (LEG_HEIGHT - 0.1f), 0.0f)));
  vec3 hinge(0.0f, RAMP_HINGE_Y, RAMP_HINGE_Z);
  mat4 ramp = glm::translate(mat4(1.0f), hinge);
  ramp = rotate(ramp, ease(rampOpen) * RAMP_ANGLE, vec3(1.0f, 0.0f, 0.0f));
  setPartTransform(rampPart, glm::translate(ramp, -hinge));
  setPartVisible(beamPart, beamOn());
}

bool Saucer::beamOn() const {
  return phase == Phase::Descending || phase == Phase::Ascending ||
         (taking && phase != Phase::Away && phase != Phase::Leaving);
}

vec3 Saucer::rampTop() const {
  return position + vec3(rotation * vec4(0.0f, RAMP_HINGE_Y, RAMP_HINGE_Z, 0.0f));
}

vec3 Saucer::rampFoot() const {
  vec3 local(0.0f, RAMP_HINGE_Y - std::sin(RAMP_ANGLE) * RAMP_LENGTH,
             RAMP_HINGE_Z + std::cos(RAMP_ANGLE) * RAMP_LENGTH);
  return position + vec3(rotation * vec4(local, 0.0f));
}

vec3 Saucer::hatch() const { return position + vec3(0.0f, LEG_HEIGHT + 0.1f, 0.0f); }

void Saucer::getLights(vector<SpotLight> &lights) const {
  if (phase == Phase::Away)
    return;
  if (beamOn()) {
    SpotLight beam;
    beam.position = position + vec3(0.0f, LEG_HEIGHT - 0.05f, 0.0f);
    beam.direction = vec3(0.0f, -1.0f, 0.0f);
    beam.color = BEAM_COLOR * 3.0f;
    beam.innerCos = std::cos(radians(28.0f));
    beam.outerCos = std::cos(radians(42.0f));
    beam.range = 60.0f;
    lights.push_back(beam);
  } else if (phase == Phase::Landed || phase == Phase::Closing || phase == Phase::Piloted) {
    lights.push_back(SpotLight::omni(position + vec3(0.0f, LEG_HEIGHT * 0.6f, 0.0f),
                                     vec3(0.25f, 0.6f, 0.55f), 9.0f));
  }
}

void Saucer::describe(vector<string> &lines) const {
  PlayableCharacter::describe(lines);
  const char *names[] = {"fuera", "llegando", "aterrizando", "en tierra", "pilotada por el jugador",
                         "cerrando la rampa", "despegando", "marchandose"};
  lines.push_back(string("Nave de Bob: ") + names[(int)phase] + (taking ? " (se lleva al jugador)" : ""));
  lines.push_back(textFormat("Patas: %.0f %%  Rampa: %.0f %%  Motor: %s  En el suelo: %s", legsOut * 100.0f,
                             rampOpen * 100.0f, engineOn ? "si" : "no", onGround ? "si" : "no"));
}

void Saucer::getProperties(vector<Property> &properties) {
  PlayableCharacter::getProperties(properties);
  properties.push_back(Property::action("Venir ahora", [this]() {
    if (phase == Phase::Away)
      enter(Phase::Arriving);
  }));
}
