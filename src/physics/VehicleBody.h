#ifndef VEHICLE_BODY
#define VEHICLE_BODY

#include <functional>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Rigid-body physics of a wheeled vehicle with a spring-damper suspension on
// every wheel (a "raycast vehicle": the wheels are not bodies, each one probes
// the floor under its mounting point). No OpenGL: it only needs a way to ask
// for the floor.
//
// The chassis has mass, inertia, a position (of its centre of mass) and an
// orientation. Each wheel hangs from an anchor on the chassis on a spring; the
// floor pushes the wheel up and the spring pushes the chassis, so the chassis
// bounces, pitches and rolls over the terrain. The tyres give the drive and
// brake forces and the sideways grip, limited by the load on each wheel.
// Units: metres, seconds, kilograms, Newtons.
class VehicleBody {
public:
  // Height of the floor at (x, z) and its upward normal; only surfaces at or
  // below maxY count. False if there is no floor there.
  typedef std::function<bool(float x, float z, float maxY, float &height,
                             glm::vec3 &normal)>
      FloorQuery;

  // What the ground is like under a wheel: the tyres' grip, how hard it is to
  // roll over it, and how fast the engine can take the vehicle on it. All are
  // multipliers of the vehicle's own Params, so 1 = as on a good road.
  struct Surface {
    float grip = 1.0f;     // of the tyre force limit and the sideways grip
    float rolling = 1.0f;  // of the rolling drag (sand: much more)
    float topSpeed = 1.0f; // of Params::maxSpeed (the terminal speed)
  };
  // The surface at a place in the world (x, z)
  typedef std::function<Surface(float x, float z)> SurfaceQuery;

  struct Wheel {
    glm::vec3 anchor; // where the suspension is fixed, in the chassis frame
    bool steered;
  };

  struct Params {
    float mass = 3000.0f;
    glm::vec3 inertia = glm::vec3(1.0f); // about the chassis axes x, y, z
    glm::vec3 centreOfMass = glm::vec3(0.0f); // in the chassis frame
    std::vector<Wheel> wheels;
    // Points of the chassis that must not go into the floor (its corners):
    // they hold it up on its side or roof if it rolls over
    std::vector<glm::vec3> bumpers;

    float wheelRadius = 0.5f;
    float gravity = 9.81f;
    // The suspension length (anchor to wheel centre) goes from fullBump to
    // fullDroop. The spring is free at fullDroop and carries the vehicle at
    // restLength; stiffness and damping follow from these.
    float fullBump = 0.15f;
    float fullDroop = 0.55f;
    float restLength = 0.35f;
    float dampingRatio = 0.7f; // 1 = critically damped

    float grip = 0.8f;      // 0..1, how much sideways slip a tyre cancels
    float friction = 1.4f;  // tyre force limit, times the load on the wheel
    float acceleration = 14.0f;  // engine, the same at any speed (m/s^2)
    float maxSpeed = 20.0f;      // terminal speed: the air drag (grows with
                                 // the square of the speed) balances the engine
    float reverseFactor = 0.4f;  // reverse terminal speed, of maxSpeed
    float braking = 16.0f;       // m/s^2
    float rolling = 0.15f;       // coasting drag (1/s)
    float maxSteer = 0.5f;       // front wheel angle, radians
    float steerFalloff = 15.0f;  // speed (m/s) at which the angle is halved
    float steerRate = 3.0f;      // radians / second

    // Self-righting (like a roly-poly toy, a "tentetieso"): the chassis is
    // always pulled back towards standing on its wheels, gently when it is a
    // bit tilted and hard once it is tilted more than freeAngle. Both are
    // angular frequencies (rad/s): the angular acceleration per radian of tilt.
    float uprightSoft = 1.5f;
    float uprightHard = 5.0f;
    float freeAngle = 0.45f;      // radians
    float uprightDamping = 0.6f;  // damping ratio of the tilting motion

    // How much of it works in the air, where there is nothing to push against
    float uprightInAir = 0.15f;

    float bumperFriction = 0.8f;  // of the chassis against the floor

    // A lost wheel (setLost: the tyre blew off): nothing holds that corner up but the bare hub, a
    // stump as much smaller than the wheel as lostDrop: the corner falls until the underside of
    // the body is nearly on the floor (the chassis' lowest edge is 0.46 above the wheels' ground
    // level: the stump is nearly a point), and scrapes along it. It rolls lostRolling times as hard and grips lostGrip of a good wheel. Each lost wheel takes
    // lostSpeed of the top speed the terrain allows (a quarter: with all four gone it is zero and
    // the engine has nothing to push), whatever the speed the vehicle has now.
    float lostDrop = 0.45f;
    float lostRolling = 1.0f;
    float lostGrip = 1.0f;
    float lostSpeed = 0.25f;
    // With all four gone the hubs scrape along the floor and stop it: the rolling drag is this
    // many times harder
    float lostScrape = 6.0f;
    // The drift (below) costs some speed: the top speed is raised by this much while a wheel is
    // missing (and not all), so that the speed it really reaches is the stated one (measured)
    float lostDragMakeUp = 1.15f;
    float lostDragMakeUpMore = 0.10f; // ...and this more for each wheel after the first
    // ...and the scraping drags it as if the wheel were turned this much (radians) towards its
    // own side, which makes the whole vehicle drift that way
    float lostSteer = 0.01f;
    // The strongest push of one contact point of the chassis, in vehicle
    // weights: a point that has sunk deep is pushed out firmly but not shot
    // out (the stage also pushes the chassis out of the floor)
    float maxBumperLoad = 3.0f;
  };

  // State of a wheel, for drawing and sound
  struct WheelState {
    float length;       // suspension length, anchor to wheel centre
    float steer;        // angle around the chassis' up axis
    bool onGround;
  };

  explicit VehicleBody(const Params &params);

  // The chassis frame's origin at `origin`, heading `yaw`, at rest with the
  // suspension settled (as if it had been standing there)
  void place(const glm::vec3 &origin, float yaw);

  // Where to ask what the ground is like under each wheel (without it, every
  // wheel is on a good road). It is asked in every physics step.
  void setSurfaceQuery(const SurfaceQuery &query) { surfaces = query; }

  // A wrecked vehicle has no wheels any more: no suspension, no tyres, no engine, and nothing rights
  // it. Only the chassis' corners touch the floor (the bumpers), so it falls, tumbles and drags on
  // them like any rigid box.
  void setWrecked(bool on) { wrecked = on; }
  bool isWrecked() const { return wrecked; }

  // Holds the vehicle still: the tyres brake it (nobody is driving it)
  void setHandbrake(bool on) { handbrake = on; }

  // throttle -1 (reverse) .. 1, steering -1 (right) .. 1 (left)
  void setInput(float throttle, float steering) {
    this->throttle = throttle;
    this->steering = steering;
  }

  // Advances dt seconds (in small steps)
  void step(double dt, const FloorQuery &floor);

  glm::vec3 getOrigin() const; // chassis frame origin, world
  glm::mat3 getRotation() const;
  glm::vec3 getCentreOfMass() const { return com; }
  glm::vec3 getVelocity() const { return velocity; }
  glm::vec3 getAngularVelocity() const { return angular; }
  float getForwardSpeed() const; // along the chassis' +z
  const std::vector<WheelState> &getWheels() const { return wheelStates; }
  bool isOnGround() const;
  const Params &getParams() const { return params; }
  // The terminal speed (Params::maxSpeed), e.g. from the debug inspector
  void setMaxSpeed(float speed) { params.maxSpeed = speed; }

  // Wheel i is gone (see Params::lostDrop...): its corner drags on the floor, the vehicle pulls
  // towards it and the top speed falls
  void setLost(size_t wheel, bool isLost) {
    if (wheel < lostWheel.size())
      lostWheel[wheel] = isLost;
  }
  bool isLost(size_t wheel) const { return wheel < lostWheel.size() && lostWheel[wheel]; }

  // Turns the chassis (e.g. to test it upside down); no other state changes
  void setOrientation(const glm::quat &q) { orientation = glm::normalize(q); }

  // A client's copy of a vehicle the server drives: puts the chassis where the server says it is
  // (the origin of its frame and its turn) and moving as it says, and a wheel as the server
  // has it. No physics is done (step is not called).
  void setState(const glm::vec3 &origin, const glm::mat3 &rotation, const glm::vec3 &v,
                const glm::vec3 &w) {
    orientation = glm::normalize(glm::quat_cast(rotation));
    com = origin + rotation * params.centreOfMass;
    velocity = v;
    angular = w;
  }
  void setWheel(size_t i, float length, float steer, bool onGround) {
    if (i < wheelStates.size()) {
      wheelStates[i].length = length;
      wheelStates[i].steer = steer;
      wheelStates[i].onGround = onGround;
    }
  }

  // For the stage to keep the vehicle inside the floor
  void setCentreOfMass(const glm::vec3 &p) { com = p; }
  void setVelocity(const glm::vec3 &v) { velocity = v; }
  void setAngularVelocity(const glm::vec3 &w) { angular = w; }

private:
  Params params;
  float stiffness, damping;       // per wheel
  float bumpStiffness, bumpDamping;
  glm::vec3 com = glm::vec3(0.0f);
  glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  glm::vec3 velocity = glm::vec3(0.0f);
  glm::vec3 angular = glm::vec3(0.0f);
  float throttle = 0.0f, steering = 0.0f;
  bool handbrake = false;
  bool wrecked = false;
  SurfaceQuery surfaces;
  float steerAngle = 0.0f;
  std::vector<WheelState> wheelStates;
  std::vector<bool> lostWheel; // per wheel (see setLost)

  void substep(float h, const FloorQuery &floor);
};

#endif
