#include "RV.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "Stage.h"
using namespace glm;

// Strongest acceleration of the RV, units / second^2
#define RV_ACCELERATION 14.0f
// Mass of the RV, kg
#define RV_MASS 3000.0f
// How the ground changes the RV (see VehicleBody::Surface; asphalt is the
// reference): on sand the tyres grip less, it is much harder to roll and the
// engine only takes it to half the speed
#define SAND_GRIP 0.75f
#define SAND_ROLLING 2.5f
#define SAND_TOP_SPEED 0.5f
// The dust a wheel throws up on sand: the particles go backwards (against the
// direction of travel) and upwards, and then fall. It starts above a walking
// pace, and the faster it goes, the more there is.
#define DUST_MIN_SPEED 1.5f       // m/s
#define DUST_RATE 45.0f           // particles per second and wheel at 10 m/s
#define DUST_MAX_RATE_SPEED 15.0f // the rate stops growing at this speed
#define DUST_BACKWARDS 0.75f      // direction: this much backwards...
#define DUST_UPWARDS 1.3f         // ...and this much upwards
#define DUST_INHERIT 0.25f        // of the RV's own velocity that they take
// Without gravity the suspension has nothing to carry
#define DEFAULT_GRAVITY 9.81f

// The RV model (rv.obj): its origin is on the ground between the wheels, the
// front is +z, the wheel axles are at height 0.5 and the chassis is
// 2.4 wide, 7.4 long and 3.3 tall
static const float HALF_TRACK = 1.2f;
static const float FRONT_AXLE = 2.4f;
static const float REAR_AXLE = -2.3f;
static const float WHEEL_RADIUS = 0.5f;
// Half the length of the RV (with the bumpers): how far it keeps from the
// edge of the floor
static const float BODY_RADIUS = 4.0f;
// The door is on the +x side, a bit behind the middle (see generate_rv.py), and
// the driver sits in the cab
static const float DOOR_Z = -0.35f;
static const float SEAT_Y = 1.2f, SEAT_Z = 1.4f;
static const float ANCHOR_HEIGHT = 0.85f; // suspension mounts, on the chassis

// The chassis, without the wheels (which hang under it, on the suspension): 2.5
// wide, 7.4 long, from its underside (clearance for slopes) to the roof
static const float CHASSIS_HALF_X = 1.25f, CHASSIS_HALF_Z = 3.7f;
static const float CHASSIS_LOW = 0.5f, CHASSIS_HIGH = 3.3f;
static std::shared_ptr<const CollisionShape> chassisShape() {
  return std::make_shared<Box>(
      vec3(CHASSIS_HALF_X, (CHASSIS_HIGH - CHASSIS_LOW) / 2, CHASSIS_HALF_Z),
      vec3(0.0f, (CHASSIS_HIGH + CHASSIS_LOW) / 2, 0.0f));
}
// Where the springy contact points are: 4 cm outside that box
static const float SKIN = 0.04f;
static const float CHASSIS_X = CHASSIS_HALF_X + SKIN;
static const float CHASSIS_Z = CHASSIS_HALF_Z + SKIN;
static const float CHASSIS_BOTTOM = CHASSIS_LOW - SKIN;
static const float CHASSIS_TOP = CHASSIS_HIGH + SKIN;

RV::RV(std::shared_ptr<Model> model)
    : PlayableCharacter(model, chassisShape()) {
  ParticleSettings settings; // sand dust: tan, soft, heavy enough to fall
  settings.lifeMin = 1.0f;
  settings.lifeMax = 1.8f;
  settings.speedMin = 2.5f;
  settings.speedMax = 6.0f;
  settings.spread = 0.5f;
  settings.sizeStart = 0.3f;
  settings.sizeEnd = 1.3f;
  settings.color = vec3(0.95f, 0.89f, 0.76f); // paler than the sand it comes from
  settings.alpha = 0.85f;
  settings.gravity = 7.0f;
  settings.drag = 1.2f;
  settings.maxParticles = 400;
  for (unsigned i = 0; i < 4; i++)
    dust.push_back(std::make_shared<ParticleEmitter>(settings, 100 + i));
}

float RV::getMass() const { return RV_MASS; }

void RV::applyCollision(const vec3 &push, const vec3 &velocityChange) {
  PlayableCharacter::applyCollision(push, velocityChange);
  if (body) {
    body->setCentreOfMass(body->getCentreOfMass() + push);
    body->setVelocity(body->getVelocity() + velocityChange);
  }
}

static VehicleBody::Surface surfaceOf(FloorMaterial material) {
  VehicleBody::Surface surface; // asphalt: as is
  if (material == FloorMaterial::Sand) {
    surface.grip = SAND_GRIP;
    surface.rolling = SAND_ROLLING;
    surface.topSpeed = SAND_TOP_SPEED;
  }
  return surface;
}

VehicleBody::Params RV::vehicleParams(float gravity, float maxSpeed) {
  VehicleBody::Params p;
  p.mass = RV_MASS;
  // Like a box of the body's size, but with the weight low (ballast and
  // engine under the floor): a short "height" for the inertia and a centre of
  // mass under the wheel axles, so it does not roll over easily
  // (m/12 * (a^2 + b^2) about each axis)
  const float w = 2.4f, h = 1.2f, l = 7.4f;
  p.inertia = vec3(p.mass / 12.0f * (h * h + l * l),
                   p.mass / 12.0f * (w * w + l * l),
                   p.mass / 12.0f * (w * w + h * h));
  p.centreOfMass = vec3(0.0f, 0.4f, 0.0f);
  p.wheelRadius = WHEEL_RADIUS;
  // front -x, front +x, rear -x, rear +x
  p.wheels.push_back({vec3(-HALF_TRACK, ANCHOR_HEIGHT, FRONT_AXLE), true});
  p.wheels.push_back({vec3(HALF_TRACK, ANCHOR_HEIGHT, FRONT_AXLE), true});
  p.wheels.push_back({vec3(-HALF_TRACK, ANCHOR_HEIGHT, REAR_AXLE), false});
  p.wheels.push_back({vec3(HALF_TRACK, ANCHOR_HEIGHT, REAR_AXLE), false});
  // The corners of the chassis box (see chassisShape), a little outside it so
  // that the springy contact works before the stage has to push the box out
  // of the floor
  for (float x : {-CHASSIS_X, CHASSIS_X})
    for (float z : {-CHASSIS_Z, CHASSIS_Z})
      for (float y : {CHASSIS_BOTTOM, CHASSIS_TOP})
        p.bumpers.push_back(vec3(x, y, z));
  p.gravity = gravity > 0.0f ? gravity : DEFAULT_GRAVITY;
  p.maxSpeed = maxSpeed;
  p.acceleration = RV_ACCELERATION;
  return p;
}

void RV::setWheelModels(std::shared_ptr<Model> negativeX,
                        std::shared_ptr<Model> positiveX) {
  for (int i = 0; i < 4; i++)
    wheelParts[i] = addPart(i % 2 == 0 ? negativeX : positiveX);
  hasWheels = true;
  placeWheels();
}

vec3 RV::seatPosition() const {
  return position + vec3(rotation * vec4(0.0f, SEAT_Y, SEAT_Z, 0.0f));
}

vec3 RV::doorPosition(float outside) const {
  return position +
         vec3(rotation * vec4(HALF_TRACK + outside, 0.0f, DOOR_Z, 0.0f));
}

// The camera yaw that looks along a world direction
static float yawOf(const vec3 &direction) {
  return std::atan2(direction.x, -direction.z);
}

float RV::headingYaw() const {
  return yawOf(vec3(rotation * vec4(0.0f, 0.0f, 1.0f, 0.0f)));
}

float RV::doorYaw() const {
  return yawOf(vec3(rotation * vec4(1.0f, 0.0f, 0.0f, 0.0f)));
}

void RV::onUse(const vec3 &playerPosition) {
  if (enterAction && !occupied)
    enterAction();
}

void RV::attachCamera(Camera *camera, float distance, float height) {
  this->camera = camera;
  setYaw(facing);
  camera->attachTo(this, distance, height);
}

void RV::followCamera() {
  if (camera)
    camera->follow();
}

void RV::control(vec2 dir, float up, float cameraYaw) {
  throttle = -dir.y; // W is forward
  steering = dir.x;  // D is right
}

void RV::update(double dt) {
  // Only the input is read here: the stage moves the RV through contactFloor()
  GameObject::update(dt);
}

// Wheel i hangs from its anchor at the length the suspension has now
void RV::placeWheels() {
  if (!hasWheels)
    return;
  const VehicleBody::Params &p = body ? body->getParams() : vehicleParams(0, 0);
  for (int i = 0; i < 4; i++) {
    float length = p.restLength; // before the first step
    float steer = 0.0f;
    if (body) {
      length = body->getWheels()[i].length;
      steer = body->getWheels()[i].steer;
    }
    const vec3 &anchor = p.wheels[i].anchor;
    mat4 local = glm::translate(mat4(1.0f), vec3(anchor.x, anchor.y - length, anchor.z));
    local = glm::rotate(local, steer, vec3(0.0f, 1.0f, 0.0f));
    setPartTransform(wheelParts[i], local);
  }
}

// Each wheel that is on sand and moving throws dust up and back from where
// its tyre meets the ground; on asphalt, or in the air, it throws nothing
void RV::updateDust(const Stage &stage) {
  vec3 horizontal(velocity.x, 0.0f, velocity.z);
  float speed = length(horizontal);
  vec3 away = speed > 1e-3f ? -horizontal / speed : vec3(0.0f); // against the travel
  const VehicleBody::Params &params = body->getParams();
  for (size_t i = 0; i < dust.size(); i++) {
    const VehicleBody::WheelState &wheel = body->getWheels()[i];
    // the point of the tyre on the ground
    vec3 anchor = params.wheels[i].anchor;
    vec3 contact = position + vec3(rotation * vec4(anchor.x,
                                                   anchor.y - wheel.length - WHEEL_RADIUS,
                                                   anchor.z, 0.0f));
    bool onSand = wheel.onGround &&
                  stage.materialAt(contact.x, contact.z) == FloorMaterial::Sand;
    ParticleEmitter &emitter = *dust[i];
    if (!onSand || speed < DUST_MIN_SPEED) {
      emitter.setRate(0.0f);
      continue;
    }
    emitter.setPosition(contact + vec3(0.0f, 0.05f, 0.0f));
    emitter.setDirection(away * DUST_BACKWARDS + vec3(0.0f, DUST_UPWARDS, 0.0f));
    emitter.setBaseVelocity(velocity * DUST_INHERIT);
    emitter.setRate(DUST_RATE * std::min(speed, DUST_MAX_RATE_SPEED) / 10.0f);
  }
}

bool RV::contactFloor(const Stage &stage, double dt) {
  if (!body) {
    body.reset(new VehicleBody(vehicleParams(gravity, maxSpeed)));
    body->place(position, facing);
  }
  // What each wheel drives on, from the stage's floor
  body->setSurfaceQuery([&stage](float x, float z) {
    return surfaceOf(stage.materialAt(x, z));
  });
  body->setHandbrake(!occupied); // an empty RV stays where it is
  body->setInput(throttle, -steering);
  body->step(dt, [&stage](float x, float z, float maxY, float &height,
                          vec3 &normal) {
    return stage.floorAt(x, z, height, &normal, maxY);
  });

  // Not a number: something went wrong, start again from the last good place
  vec3 centre = body->getCentreOfMass(), v = body->getVelocity();
  if (!(std::isfinite(centre.x + centre.y + centre.z + v.x + v.y + v.z))) {
    body->place(position, facing);
    return true;
  }
  // Don't leave the floor: the whole RV has to stay on it
  stage.keepInsideFloor(centre, v, BODY_RADIUS);
  body->setCentreOfMass(centre);
  body->setVelocity(v);

  position = body->getOrigin();
  rotation = mat4(body->getRotation());
  velocity = body->getVelocity();
  grounded = body->isOnGround();
  placeWheels();
  updateDust(stage);
  return true;
}
