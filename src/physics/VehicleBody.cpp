#include "VehicleBody.h"

#include <cmath>

using namespace glm;

// Physics step: small and fixed, the springs are stiff
#define SUBSTEP (1.0f / 240.0f)
#define MAX_SUBSTEPS 24
// The angular velocity decays by this much per second (stability)
#define ANGULAR_DAMPING 0.4f
// Slope (cos of its angle) from which the wheel doesn't touch the floor
#define MIN_UP_DOT 0.25f

VehicleBody::VehicleBody(const Params &p) : params(p) {
  // Spring that carries a quarter of the weight at restLength, damped
  float load = params.mass * params.gravity / params.wheels.size();
  stiffness = load / (params.fullDroop - params.restLength);
  damping = 2.0f * params.dampingRatio *
            sqrt(stiffness * params.mass / params.wheels.size());
  // The bump stop and the bumpers are much stiffer than the spring
  bumpStiffness = 100.0f * stiffness;
  bumpDamping = 6.0f * damping;
  wheelStates.assign(params.wheels.size(),
                     WheelState{params.restLength, 0.0f, false});
  lostWheel.assign(params.wheels.size(), false);
}

void VehicleBody::place(const vec3 &origin, float yaw) {
  orientation = angleAxis(yaw, vec3(0.0f, 1.0f, 0.0f));
  com = origin + mat3_cast(orientation) * params.centreOfMass;
  velocity = vec3(0.0f);
  angular = vec3(0.0f);
  steerAngle = 0.0f;
  for (WheelState &w : wheelStates) {
    w.length = params.restLength;
    w.steer = 0.0f;
  }
}

vec3 VehicleBody::getOrigin() const {
  return com - mat3_cast(orientation) * params.centreOfMass;
}

mat3 VehicleBody::getRotation() const { return mat3_cast(orientation); }

float VehicleBody::getForwardSpeed() const {
  return dot(velocity, mat3_cast(orientation) * vec3(0.0f, 0.0f, 1.0f));
}

bool VehicleBody::isOnGround() const {
  for (const WheelState &w : wheelStates)
    if (w.onGround)
      return true;
  return false;
}

void VehicleBody::step(double dt, const FloorQuery &floor) {
  int steps = (int)ceil(dt / SUBSTEP);
  if (steps > MAX_SUBSTEPS)
    steps = MAX_SUBSTEPS; // a long frame: the vehicle just runs slower
  if (steps < 1)
    return;
  float h = (float)dt / steps;
  if (h > SUBSTEP)
    h = SUBSTEP;
  for (int i = 0; i < steps; i++)
    substep(h, floor);
}

void VehicleBody::substep(float h, const FloorQuery &floor) {
  const mat3 R = mat3_cast(orientation);
  const vec3 up = R * vec3(0.0f, 1.0f, 0.0f);
  const vec3 fwd = R * vec3(0.0f, 0.0f, 1.0f);
  const vec3 origin = com - R * params.centreOfMass;
  const float g = params.gravity;
  const float wheelMass = params.mass / params.wheels.size();

  vec3 force(0.0f, -params.mass * g, 0.0f);
  vec3 torque(0.0f);
  bool touching = false; // the wheels or the chassis are on the floor
  auto apply = [&](const vec3 &f, const vec3 &point) {
    force += f;
    torque += cross(point - com, f);
  };

  // Steering: the angle follows the input, and is smaller at speed
  float speed = dot(velocity, fwd);
  float wanted = steering * params.maxSteer /
                 (1.0f + fabs(speed) / params.steerFalloff);
  float step = params.steerRate * h;
  steerAngle += clamp(wanted - steerAngle, -step, step);

  // The ground under each wheel, and what the engine can do with it: the top
  // speed is the mean over the wheels (a vehicle half on sand is half slowed)
  std::vector<Surface> surface(params.wheels.size());
  float topSpeed = 0.0f, rollingFactor = 0.0f;
  for (size_t i = 0; i < params.wheels.size(); i++) {
    if (surfaces) {
      vec3 anchor = origin + R * params.wheels[i].anchor;
      surface[i] = surfaces(anchor.x, anchor.z);
    }
    topSpeed += surface[i].topSpeed / params.wheels.size();
    rollingFactor += surface[i].rolling / params.wheels.size();
  }
  // Each lost wheel takes a quarter of the top speed the terrain allows (all four: none)
  int lostCount = 0;
  for (bool lost : lostWheel)
    lostCount += lost ? 1 : 0;
  float lostShare = max(1.0f - params.lostSpeed * lostCount, 0.0f) * (lostCount > 0 ? params.lostDragMakeUp + params.lostDragMakeUpMore * (lostCount - 1) : 1.0f);
  bool noEngine = lostCount >= (int)lostWheel.size();
  // (with no wheel left the engine does not push, but the air still opposes the motion as it did: a
  // top speed of nothing would give the drag a huge coefficient and blow the velocity up)
  float maxSpeed = max(params.maxSpeed * topSpeed * (noEngine ? 1.0f : lostShare), 0.1f);

  // maxSpeed is the terminal speed: the engine pushes with the same force at
  // any speed and what opposes the motion grows with it, linearly (the tyres'
  // rolling drag, in the wheels' loop) and with the square of the speed (the
  // air). The air drag coefficient is the one that balances the engine at
  // maxSpeed on flat ground, so the vehicle gets there asymptotically (never
  // past it, unless it goes downhill or is pushed).
  float rollingAtMax = params.rolling * rollingFactor * maxSpeed;
  float airDrag = max(params.acceleration - rollingAtMax,
                      0.1f * params.acceleration) / (maxSpeed * maxSpeed); // 1/m
  vec3 flat(velocity.x, 0.0f, velocity.z);
  force -= params.mass * airDrag * glm::length(flat) * flat;

  // Engine and brake: the total force along the heading, shared by the wheels
  // that touch the floor
  float drive = 0.0f;
  if (noEngine) {
    // (no wheel left to push with)
  } else if (throttle > 0.0f) {
    if (speed < -0.5f)
      drive = params.mass * params.braking * throttle; // brakes
    else
      drive = params.mass * params.acceleration * throttle;
  } else if (throttle < 0.0f) {
    if (speed > 0.5f)
      drive = params.mass * params.braking * throttle; // brakes
    else {
      // Reverse gear: just enough force for the reverse top speed
      float reverseMax = maxSpeed * params.reverseFactor;
      float reverseForce = airDrag * reverseMax * reverseMax +
                           params.rolling * rollingFactor * reverseMax;
      drive = params.mass * reverseForce * throttle;
    }
  }
  float perWheel = drive / params.wheels.size();

  for (size_t i = 0; i < params.wheels.size(); i++) {
    const Wheel &wheel = params.wheels[i];
    WheelState &state = wheelStates[i];
    state.steer = wheel.steered ? steerAngle : 0.0f;
    state.onGround = false;
    state.length = params.fullDroop;
    if (wrecked)
      continue; // (the wheels are gone)

    vec3 anchor = origin + R * wheel.anchor;
    float groundY;
    vec3 n;
    if (!floor(anchor.x, anchor.z, anchor.y, groundY, n))
      continue;
    float height = dot(anchor - vec3(anchor.x, groundY, anchor.z), n);
    float upDot = dot(up, n);
    if (upDot < MIN_UP_DOT)
      continue;
    // Suspension length at which the wheel just touches the floor (a lost wheel's hub is smaller)
    float radius = params.wheelRadius - (lostWheel[i] ? params.lostDrop : 0.0f);
    float touch = (height - radius) / upDot;
    float droop = params.fullDroop;
    if (touch >= droop)
      continue; // in the air, hanging from the spring

    float length = max(touch, params.fullBump);
    float bumped = (params.fullBump - touch) * upDot; // into the bump stop
    state.length = length;
    state.onGround = true;
    touching = true;

    vec3 contact = anchor - up * length - n * radius;
    vec3 pointVelocity = velocity + cross(angular, contact - com);
    float approach = -dot(pointVelocity, n);
    float load = stiffness * (droop - length) + damping * approach;
    if (bumped > 0.0f)
      load += bumpStiffness * bumped + bumpDamping * max(approach, 0.0f);
    load = max(load, 0.0f);

    // Tyre: along the wheel's heading and sideways, in the floor's plane
    vec3 heading = fwd;
    float turned = wheel.steered ? state.steer : 0.0f;
    if (lostWheel[i]) { // the vehicle drifts towards the lost wheel's side (+x is its left): a front
                       // wheel turns that way, a rear one the other (it steers the tail)
      float towards = wheel.anchor.x > 0.0f ? 1.0f : -1.0f;
      turned += (wheel.anchor.z > 0.0f ? towards : -towards) * params.lostSteer;
    }
    if (turned != 0.0f)
      heading = angleAxis(turned, up) * fwd;
    heading -= n * dot(heading, n);
    heading = normalize(heading);
    vec3 side = normalize(cross(n, heading));
    float along = dot(pointVelocity, heading);
    float across = dot(pointVelocity, side);
    Surface ground = surface[i];
    if (lostWheel[i]) {
      ground.rolling *= noEngine ? params.lostScrape : params.lostRolling;
      ground.grip *= params.lostGrip;
    }
    float alongForce =
        perWheel - params.rolling * ground.rolling * wheelMass * along;
    if (handbrake) // proportional so it stops the wheel instead of reversing it
      alongForce -= clamp(wheelMass * along / h,
                          -params.mass * params.braking / params.wheels.size(),
                          params.mass * params.braking / params.wheels.size());
    float sideForce = -params.grip * ground.grip * wheelMass * across / h;
    vec3 tyre = heading * alongForce + side * sideForce;
    float limit = params.friction * ground.grip * load;
    float tyreLength = glm::length(tyre);
    if (tyreLength > limit)
      tyre *= limit / tyreLength;

    apply(n * load + tyre, contact);
  }

  // The chassis can't go into the floor either
  for (const vec3 &bumper : params.bumpers) {
    vec3 point = origin + R * bumper;
    float groundY;
    vec3 n;
    if (!floor(point.x, point.z, point.y + 0.5f, groundY, n))
      continue;
    float depth = dot(vec3(point.x, groundY, point.z) - point, n);
    if (depth <= 0.0f)
      continue;
    touching = true;
    vec3 pointVelocity = velocity + cross(angular, point - com);
    float load = bumpStiffness * depth +
                 bumpDamping * max(-dot(pointVelocity, n), 0.0f);
    load = clamp(load, 0.0f, params.maxBumperLoad * params.mass * g);
    // Dragging on the floor: it doesn't slide for ever on its side or roof
    vec3 slide = pointVelocity - n * dot(pointVelocity, n);
    vec3 drag = -slide * (params.mass / params.bumpers.size()) / h * 0.2f;
    float limit = params.bumperFriction * load, dragLength = glm::length(drag);
    if (dragLength > limit)
      drag *= limit / dragLength;
    apply(n * load + drag, point);
  }

  // Integrate (semi-implicit Euler)
  velocity += force / params.mass * h;
  mat3 inertia = R * mat3(params.inertia.x, 0, 0, 0, params.inertia.y, 0, 0, 0,
                          params.inertia.z) * transpose(R);
  vec3 spin = cross(angular, inertia * angular);
  angular += inverse(inertia) * (torque - spin) * h;

  // Self-righting: an angular acceleration about the axis that brings the
  // chassis' up towards the world's up, bigger the more it is tilted, and
  // damped so it settles on its wheels instead of swinging for ever
  float cosTilt = clamp(dot(up, vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f);
  float tilt = acos(cosTilt);
  vec3 axis = cross(up, vec3(0.0f, 1.0f, 0.0f));
  float axisLength = glm::length(axis);
  if (axisLength > 1e-4f)
    axis /= axisLength;
  else // exactly upside down: any way over will do
    axis = cosTilt < 0.0f ? fwd : vec3(0.0f);
  float excess = max(tilt - params.freeAngle, 0.0f);
  float frequency = params.uprightSoft +
                    (params.uprightHard - params.uprightSoft) *
                        min(excess / 0.3f, 1.0f);
  vec3 tilting = angular - up * dot(angular, up); // everything but the yaw
  float strength = wrecked ? 0.0f : (touching ? 1.0f : params.uprightInAir); // (a wreck stays as it falls)
  angular += (axis * (params.uprightSoft * params.uprightSoft * tilt +
                      params.uprightHard * params.uprightHard * excess) -
              tilting * (2.0f * params.uprightDamping * frequency)) *
             (strength * h);
  angular *= exp(-ANGULAR_DAMPING * h);
  com += velocity * h;
  orientation = normalize(orientation +
                          0.5f * h * quat(0.0f, angular.x, angular.y,
                                          angular.z) * orientation);
}
