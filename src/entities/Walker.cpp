#include "Walker.h"

#include <cmath>

#include "Camera.h"
#include "Gfx.h"
#include "SeatedPose.h"

using namespace glm;

// How quickly the walker reaches the speed it wants (see steerTowards)
#define RESPONSIVENESS 12.0f
#define WALK_ACCELERATION 30.0f // units / second^2
// Running: times the walking speed (maxSpeed, 4 m/s by default: 7 m/s)
#define RUN_FACTOR 1.75f

// The flashlight: a narrower and shorter beam than the RV's headlights
static const float FLASHLIGHT_RIGHT = 0.2f;  // metres from the eyes, to the right
static const float FLASHLIGHT_DOWN = 0.25f;  // ...and below them
static const float FLASHLIGHT_HEIGHT = 1.35f; // above the feet, without a camera
static const vec3 FLASHLIGHT_COLOR = vec3(1.0f, 0.95f, 0.8f);
static const float FLASHLIGHT_INNER = 0.96f; // ~16 degrees of full light
static const float FLASHLIGHT_OUTER = 0.86f; // fades out at ~31 degrees
static const float FLASHLIGHT_RANGE = 28.0f;

void Walker::attachCamera(Camera *camera, float distance, float height) {
  this->camera = camera;
  // In first person the camera is inside the model: don't draw it
  setVisible(distance > 0.0f);
  camera->attachTo(this, distance, height);
}

void Walker::followCamera() {
  if (camera)
    camera->follow();
}

void Walker::control(vec2 dir, float up, float cameraYaw) {
  // dir is in the camera's frame (x right, y backwards): turn it by the
  // camera heading, same convention as Camera::move
  heading = dir == vec2(0.0f) ? vec2(0.0f) : rotate(normalize(dir), cameraYaw);
  vertical = up;
}

void Walker::update(double dt) {
  if (heading != vec2(0.0f)) {
    facing = std::atan2(heading.x, heading.y);
    setYaw(facing);
  }
  // standing still it breathes in its idle pose (the others' penguins, seen from outside)
  if (aniModel && !sitting)
    aniModel->setIdle(length(vec2(velocity.x, velocity.z)) < 0.3f && heading == vec2(0.0f));
  float speed = running ? maxSpeed * RUN_FACTOR : maxSpeed;
  vec3 wanted = vec3(heading.x, 0.0f, heading.y) * speed;
  // Under gravity only the stage decides its height
  wanted.y = gravity > 0.0f ? velocity.y : vertical * speed;
  maxAcceleration = WALK_ACCELERATION;
  steerTowards(wanted, RESPONSIVENESS);

  // maxSpeed is the walking speed, and the parent caps the velocity at it:
  // while running the cap is the running speed
  float walkSpeed = maxSpeed;
  maxSpeed = speed;
  PlayableCharacter::update(dt);
  maxSpeed = walkSpeed;
}

void Walker::getFlashlight(std::vector<SpotLight> &lights) const {
  if (!flashlightOn)
    return;
  SpotLight light;
  if (camera) {
    light.direction = normalize(camera->getForward());
    light.position = camera->getPosition() + camera->getRight() * FLASHLIGHT_RIGHT -
                     camera->getUp() * FLASHLIGHT_DOWN;
  } else if (hasLook) {
    // (the camera's forward: see Camera::getForward)
    float cp = std::cos(lookPitch);
    light.direction = vec3(cp * std::sin(lookYaw), -std::sin(lookPitch), -cp * std::cos(lookYaw));
    light.position = position + vec3(0.0f, FLASHLIGHT_HEIGHT, 0.0f);
  } else {
    light.direction = vec3(std::sin(facing), 0.0f, std::cos(facing));
    light.position = position + vec3(0.0f, FLASHLIGHT_HEIGHT, 0.0f);
  }
  light.color = FLASHLIGHT_COLOR;
  light.innerCos = FLASHLIGHT_INNER;
  light.outerCos = FLASHLIGHT_OUTER;
  light.range = FLASHLIGHT_RANGE;
  lights.push_back(light);
}

void Walker::writeNetState(NetWriter &out) const {
  out.boolean(flashlightOn);
  out.f32(lookYaw);
  out.f32(lookPitch);
  out.boolean(running);
}

void Walker::readNetState(NetReader &in) {
  bool light = in.boolean();
  float yaw = in.f32(), pitch = in.f32();
  bool run = in.boolean();
  if (!in.isOk())
    return;
  flashlightOn = light;
  running = run;
  if (!camera) // (our own walker looks where our camera does)
    setLook(yaw, pitch);
}

void Walker::setHeldModel(std::shared_ptr<Model> model) {
  heldPart = addPart(model);
  setPartVisible(heldPart, false);
  hasHeld = true;
}

void Walker::sit(bool driver, const vec3 &grip, double dt) {
  sitting = true;
  facing = getHeading();
  if (hasHeld)
    setPartVisible(heldPart, driver);
  // (the server draws nothing: only the clients pose it)
  if (Gfx::headless || !aniModel)
    return;
  if (!seated)
    seated = std::make_shared<SeatedPose>(aniModel);
  seated->setDriver(driver);
  seated->setGrip(grip);
  seated->step(dt);
  seated->apply();
  if (hasHeld)
    setPartTransform(heldPart, seated->canTransform());
}

void Walker::standUp() {
  if (!sitting)
    return;
  sitting = false;
  if (hasHeld)
    setPartVisible(heldPart, false);
  if (aniModel)
    aniModel->usePlayedAnimation();
  setYaw(facing); // (upright again: the seat turned it with the vehicle)
}
