#include "RV.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "Explosion.h"
#include "Stage.h"
#include "TextFormat.h"
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
// forest floor (moss, needles): firmer than sand, still slower than the road
#define GRASS_GRIP 0.85f
#define GRASS_ROLLING 1.8f
#define GRASS_TOP_SPEED 0.65f
// The dust a wheel throws up on sand: the particles go backwards (against the
// direction of travel) and upwards, and then fall. It starts above a walking
// pace, and the faster it goes, the more there is.
#define DUST_MIN_SPEED 1.5f       // m/s
#define DUST_RATE 45.0f           // particles per second and wheel at 10 m/s
#define DUST_MAX_RATE_SPEED 15.0f // the rate stops growing at this speed
#define DUST_BACKWARDS 0.75f      // direction: this much backwards...
#define DUST_UPWARDS 1.3f         // ...and this much upwards
#define DUST_INHERIT 0.25f        // of the RV's own velocity that they take
// Individual grains of sand: far fewer and smaller than the dust, dark, opaque
// and heavy, so they are flung out faster, in a wider cone, and drop quickly
#define GRAIN_RATE 70.0f          // particles per second and wheel at 10 m/s
#define GRAIN_BACKWARDS 0.6f
#define GRAIN_UPWARDS 1.0f
#define GRAIN_INHERIT 0.35f
// Headlight faults: how long the lamps flicker, and how long each state of the
// flicker lasts (seconds)
#define FLICKER_MIN 0.6f
#define FLICKER_MAX 2.2f
#define FLICKER_STEP_MIN 0.03f
#define FLICKER_STEP_MAX 0.16f
// Without gravity the suspension has nothing to carry
#define DEFAULT_GRAVITY 9.81f

// The RV model (rv.obj): its origin is on the ground between the wheels, the
// front is +z, the wheel axles are at height 0.5 and the chassis is
// 2.4 wide, 7.4 long and 3.3 tall
// Headlights (rv.obj: the lamps are at x = +-1.0, y = 0.9 and 1.08, on the front)
static const float HEADLIGHT_X = 1.0f;
static const float HEADLIGHT_Y = 0.99f;
static const float HEADLIGHT_Z = 3.6f;
static const float HEADLIGHT_PITCH = -0.06f; // aims a little downwards
static const char *START_SOUND = "../assets/rv/engine_start.wav"; // the starter cranking

// The driver's eyes (Cockpit view): left-hand drive, on the door's side
static const float EYE_X = 0.45f, EYE_Y = 2.4f, EYE_Z = 1.7f;

// How much of the vehicle's roll (leaning to a side) the cockpit view follows:
// all of its heading and pitch, but only part of the roll, which on the dunes
// is big and makes the horizon swing
static const float COCKPIT_ROLL = 0.5f;

// The cockpit models (assets/rv/dashboard_mounts.json, written by generate_dashboard.py:
// keep them the same). Each model's own axes x, y, z in the RV's frame, the same for the
// key and the needles, and where its origin goes.
static const vec3 PANEL_X(-1.0f, 0.0f, 0.0f);
static const vec3 PANEL_Y(0.0f, 0.86691f, 0.49847f);
static const vec3 PANEL_Z(0.0f, 0.49847f, -0.86691f); // out of the panel, to the driver
static const vec3 KEY_ORIGIN(0.35500f, 1.65545f, 2.62828f);
static const vec3 SPEED_NEEDLE_ORIGIN(0.70000f, 1.74017f, 2.67353f);
static const vec3 FUEL_NEEDLE_ORIGIN(0.52000f, 1.74017f, 2.67353f);
// Where the dashboard's lights are (dashboard_mounts.json: dashboard_lights): one on each
// glowing component, with its range, and how they shine: dim and orange
struct DashLight {
  vec3 position;
  float range;
};
static const DashLight DASH_LIGHTS[5] = {
    {vec3(0.70f, 1.74865f, 2.65880f), 0.42f},     // the speedometer
    {vec3(0.52f, 1.74865f, 2.65880f), 0.42f},     // the fuel gauge
    {vec3(0.888f, 1.70248f, 2.64379f), 0.30f},    // the digital display
    {vec3(0.265f, 1.76718f, 2.68675f), 0.32f},    // the pilot lamps, first column
    {vec3(0.145f, 1.76718f, 2.68675f), 0.32f}};   // ... second column
static const vec3 DASH_LIGHT_COLOR(0.55f, 0.25f, 0.05f);
// The needles' own angles (degrees about their +z) at the start and at the end of the
// scale, and the speed (m/s) at the end of the speedometer
static const float SPEED_ANGLES[2] = {135.0f, -135.0f};
static const float FUEL_ANGLES[2] = {60.0f, -60.0f};
static const float SPEEDOMETER_MAX = 25.0f;
// The steering wheel (assets/rv/steering_wheel_mounts.json, written by generate_steering_wheel.py:
// keep them the same). Its axes in the RV's frame (z = the steering axis, towards the driver)
// and its origin, the hub's centre. It turns STEERING_RATIO times the front wheels' angle.
static const vec3 WHEEL_X(-1.0f, 0.0f, 0.0f);
static const vec3 WHEEL_Y(0.0f, 0.57358f, 0.81915f);
static const vec3 WHEEL_Z(0.0f, 0.81915f, -0.57358f);
static const vec3 WHEEL_ORIGIN(0.45f, 1.58f, 2.28f);
static const float STEERING_RATIO = 3.5f;
static const float KEY_ON_ANGLE = -40.0f; // turned clockwise, seen from the driver
// Fuel (0..1 of the tank) used per metre driven: the consumption is proportional to the
// speed (the distance covered in a frame is speed * dt). A full tank is 12 km.
static const float FUEL_PER_METER = 1.0f / 12000.0f; // (the default of RV::setFuelPerMeter)
// Each press of the engine key is one try, that fails with this chance
static const float START_FAIL_CHANCE = 0.4f;
// The wreck: a crash this many times harder than the least that breaks the windshield wrecks the
// front; the fuse of the explosion lasts between these (s); the blast reaches this far (m); and
// the engine bay is here, in the frame of rv.obj
static const float WRECK_SEVERITY = 2.0f;
static const float FUSE_MIN = 0.1f, FUSE_MAX = 100.0f;
static const float BLAST_RADIUS = 10.0f, BANG_VOLUME = 3.5f, FLASH_TIME = 0.6f;
static const vec3 ENGINE_BAY(0.0f, 1.35f, 3.0f);
// The handbrake (assets/rv/generate_handbrake.py: keep them the same): where the lever's pivot is
// in the RV's frame, how far the lever tips forward from upright (degrees) pulled up and released,
// and how fast it swings (rad/s)
static const vec3 HANDBRAKE_PIVOT(0.12f, 0.62f, 1.75f);
static const float HANDBRAKE_ENGAGED = 15.0f, HANDBRAKE_RELEASED = 55.0f, HANDBRAKE_SWING = 4.0f;
// The fire alarm: the lamp (the centre of dashboard_alarm.obj, written by generate_dashboard.py), how
// long a blink and its beep last (s), and how loud the beep is
static const vec3 ALARM_LAMP(0.145f, 1.8572f, 2.759f);
static const float ALARM_PERIOD = 0.6f, ALARM_VOLUME = 0.7f;
static std::shared_ptr<AudioClip> alarmClip();
// The two mirrors (generate_rv.py, turned_box: the heads of the side mirrors, 0.22 x 0.42 x 0.12, each
// turned as a whole MIRROR_YAW about the vertical and MIRROR_PITCH about its own x, angles that bounce
// the driver's line of sight straight back). Mirror 0 is the driver's (+x), mirror 1 the passenger's
// (-x). The glass is a flat quad lying on the rear face of a head, MIRROR_HALF wide and tall, and the
// picture on it is the view of a camera at the glass: MIRROR_FOV degrees (vertical), more than a flat
// mirror would show, like a convex one.
static const float MIRROR_X = 1.51f, MIRROR_Y = 2.01f, MIRROR_Z = 3.38f;
static const float MIRROR_YAW[2] = {14.0f, -22.5f}, MIRROR_PITCH[2] = {4.8f, 3.7f}; // degrees (signed)
static const float MIRROR_FOV = 30.0f;
static const float MIRROR_DEPTH = 0.06f, MIRROR_PROUD = 0.004f; // the face is 6 cm from the centre
static const vec2 MIRROR_HALF(0.095f, 0.19f);
static vec3 mirrorCentre(int side) { return vec3(side == 0 ? MIRROR_X : -MIRROR_X, MIRROR_Y, MIRROR_Z); }
// A head's own frame in the RV's: its x (along the glass), y (up it) and z (the rear face looks
// along -z)
static mat3 mirrorTurn(int side) {
  return mat3(glm::rotate(mat4(1.0f), radians(MIRROR_YAW[side]), vec3(0.0f, 1.0f, 0.0f)) *
              glm::rotate(mat4(1.0f), radians(MIRROR_PITCH[side]), vec3(1.0f, 0.0f, 0.0f)));
}
// How fast the needles and the key follow (1/s)
static const float NEEDLE_RATE = 6.0f, KEY_RATE = 8.0f;

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
    : PlayableCharacter(model, chassisShape()), random(std::random_device()()) {
  engineSim.setSeed((uint32_t)random()); // (so that the starts differ from run to run)
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

  ParticleSettings grain; // sand grains: tiny, dark, opaque, heavy
  grain.lifeMin = 0.5f;
  grain.lifeMax = 1.0f;
  grain.speedMin = 3.0f;
  grain.speedMax = 8.0f;
  grain.spread = 0.8f;
  grain.sizeStart = 0.035f;
  grain.sizeEnd = 0.03f;
  grain.color = vec3(0.50f, 0.38f, 0.24f); // darker than the sand: wet-looking grains
  grain.alpha = 1.0f;
  grain.fadeStart = 0.85f; // solid until they land
  grain.gravity = 16.0f;
  grain.drag = 0.25f;
  grain.maxParticles = 300;
  for (unsigned i = 0; i < 4; i++)
    grains.push_back(std::make_shared<ParticleEmitter>(grain, 200 + i));

  ParticleSettings flames; // the burning front: hot, it rises
  flames.lifeMin = 0.3f;
  flames.lifeMax = 0.7f;
  flames.speedMin = 2.0f;
  flames.speedMax = 4.0f;
  flames.spread = 0.5f;
  flames.sizeStart = 0.5f;
  flames.sizeEnd = 1.1f;
  flames.color = vec3(1.0f, 0.5f, 0.1f);
  flames.alpha = 0.9f;
  flames.gravity = -4.0f;
  flames.drag = 1.5f;
  flames.maxParticles = 200;
  ParticleSettings smoke;
  smoke.lifeMin = 1.5f;
  smoke.lifeMax = 3.0f;
  smoke.speedMin = 1.0f;
  smoke.speedMax = 2.5f;
  smoke.spread = 0.4f;
  smoke.sizeStart = 0.4f;
  smoke.sizeEnd = 2.2f;
  smoke.color = vec3(0.12f, 0.11f, 0.10f);
  smoke.alpha = 0.7f;
  smoke.gravity = -1.5f;
  smoke.drag = 0.8f;
  smoke.maxParticles = 160;
  fire.push_back(std::make_shared<ParticleEmitter>(flames, 300));
  fire.push_back(std::make_shared<ParticleEmitter>(smoke, 301));
  blast = makeExplosionEmitters(302);
}

float RV::getMass() const { return RV_MASS; }

void RV::applyCollision(const vec3 &push, const vec3 &velocityChange) {
  float speedBefore = body ? body->getForwardSpeed() : 0.0f;
  PlayableCharacter::applyCollision(push, velocityChange);
  if (body) {
    body->setCentreOfMass(body->getCentreOfMass() + push);
    body->setVelocity(body->getVelocity() + velocityChange);
  }
  // Was it a violent frontal crash? The collision's own push tells where it came from (the
  // change of speed has the same direction: both point away from what it hit)
  float speedAfter = body ? body->getForwardSpeed() : speedBefore;
  vec3 away = length(vec2(velocityChange.x, velocityChange.z)) > 1e-3f ? velocityChange : push;
  impact.onCollision(speedBefore, speedAfter, vec3(rotation * vec4(0.0f, 0.0f, 1.0f, 0.0f)),
                     away);
  handleImpact();
}

// The slanted front face of the body (generate_rv.py: A = (z 3.55, y 1.7) to B = (z 3.0, y 2.95)),
// and the middle of the glass in it (T0 = 0.08 to T1 = 0.85 along it)
static const float GLASS_MID_Z = 3.55f - 0.55f * 0.465f, GLASS_MID_Y = 1.7f + 1.25f * 0.465f;
static const float SLOPE_UP_Y = 1.25f / 1.3658f, SLOPE_UP_Z = -0.55f / 1.3658f; // unit vector up it
// Where its front is, for isInFront: from the bumper ahead
static const float FRONT_Z = 3.55f, FRONT_REACH = 1.9f, FRONT_HALF_WIDTH = 1.7f;

void RV::windshieldFrame(vec3 &center, vec3 &up, vec3 &normal) const {
  center = position + vec3(rotation * vec4(0.0f, GLASS_MID_Y, GLASS_MID_Z, 0.0f));
  up = vec3(rotation * vec4(0.0f, SLOPE_UP_Y, SLOPE_UP_Z, 0.0f));
  // the outward normal: perpendicular to the slope, pointing forwards and up
  normal = vec3(rotation * vec4(0.0f, -SLOPE_UP_Z, SLOPE_UP_Y, 0.0f));
}

// The side profile of the body (generate_rv.py: PROF, (z, y), counter-clockwise) and its half width
static const float BODY_PROFILE[7][2] = {{-3.55f, 0.55f}, {3.55f, 0.55f}, {3.55f, 1.7f}, {3.0f, 2.95f},
                                         {2.85f, 3.05f}, {-3.4f, 3.05f}, {-3.55f, 2.9f}};
static const float BODY_HALF_WIDTH = 1.2f;

void RV::pushOutOfBody(vec3 &point, float radius) const {
  vec3 local = vec3(transpose(mat3(rotation)) * (point - position));
  // The body is the profile extruded along x: a convex shape, the planes of its edges (outward
  // normals) and its two sides. The sphere is inside if it is inside all of them; then it
  // leaves through the nearest.
  vec3 bestNormal(0.0f);
  float bestDistance = -1e30f;
  auto consider = [&](const vec3 &normal, float distance) {
    if (distance > bestDistance) {
      bestDistance = distance;
      bestNormal = normal;
    }
  };
  for (int i = 0; i < 7; i++) {
    const float *a = BODY_PROFILE[i], *b = BODY_PROFILE[(i + 1) % 7];
    vec2 edge(b[0] - a[0], b[1] - a[1]);
    vec2 n = normalize(vec2(edge.y, -edge.x)); // outward for a counter-clockwise profile (z, y)
    consider(vec3(0.0f, n.y, n.x), n.x * (local.z - a[0]) + n.y * (local.y - a[1]));
  }
  consider(vec3(1.0f, 0.0f, 0.0f), local.x - BODY_HALF_WIDTH);
  consider(vec3(-1.0f, 0.0f, 0.0f), -local.x - BODY_HALF_WIDTH);
  if (bestDistance >= radius)
    return; // outside (or touching) in at least one direction
  local += bestNormal * (radius - bestDistance);
  point = position + vec3(rotation * vec4(local, 0.0f));
}

bool RV::isInFront(const vec3 &p) const {
  vec3 local = vec3(transpose(mat3(rotation)) * (p - position));
  return local.z > FRONT_Z - FRONT_REACH && local.z < FRONT_Z + 2.0f * FRONT_REACH &&
         std::fabs(local.x) < FRONT_HALF_WIDTH;
}

void RV::setWindshieldModels(std::shared_ptr<Model> intact, std::shared_ptr<Model> broken) {
  windshieldPart = addPart(intact);
  brokenWindshieldPart = addPart(broken);
  hasWindshield = true;
  updateWindshieldParts();
}

// The flag goes up and the broken windshield is shown instead of the intact one
void RV::breakWindshield() {
  damagedWindshield = true;
  impact.clear();
  updateWindshieldParts();
}

void RV::handleImpact() {
  if (!impact.violent())
    return;
  if (!damagedWindshield) { // (this much at once; the rest when the crash is over)
    damagedWindshield = true;
    updateWindshieldParts();
  }
  if (impact.timerRunning())
    return; // it may still get worse
  bool severe = impact.severity() >= WRECK_SEVERITY;
  impact.clear();
  if (severe)
    wreck();
}

// How the front of the body is crushed (rv.obj's frame, the front towards +z): from the cab
// forward, more and more, and less up by the windshield (the cab does not fold into the driver);
// with some jagged dents, a sag and a squeeze from the sides. Continuous, so it never tears.
static vec3 crumple(const vec3 &p) {
  float t = glm::clamp((p.z - 2.0f) / 1.9f, 0.0f, 1.0f);
  float s = t * t;
  if (s <= 0.0f)
    return p;
  float up = glm::smoothstep(1.4f, 2.6f, p.y);
  float low = 1.0f - 0.8f * up;
  float dentZ = std::sin(5.3f * p.x + 1.7f * p.y) * std::sin(4.1f * p.z + 2.3f * p.x) +
                0.5f * std::sin(11.7f * p.y + 3.1f * p.z);
  float dentY = std::sin(6.1f * p.z + 2.9f * p.x) * std::cos(3.3f * p.y);
  float dentX = std::sin(4.7f * p.y + 5.9f * p.z);
  vec3 q = p;
  q.z -= s * (1.2f * low + 0.22f * dentZ);
  q.y -= s * (0.30f * low + 0.12f * dentY);
  q.x = q.x * (1.0f - 0.12f * s) + 0.15f * s * dentX + 0.12f * s * (p.y - 1.5f) * 0.3f;
  return q;
}

void RV::wreck() {
  if (wrecked)
    return;
  wrecked = true;
  if (!damagedWindshield) {
    damagedWindshield = true;
    updateWindshieldParts();
  }
  // crumpled copies of everything at the front (the original models are not touched: other RVs
  // share them)
  parts[0].model = parts[0].model->deformed(crumple);
  if (hasWindshield)
    parts[brokenWindshieldPart].model = parts[brokenWindshieldPart].model->deformed(crumple);
  if (hasGlow)
    parts[glowPart].model = parts[glowPart].model->deformed(crumple);
  if (hasCockpit) // the dashboard is inside the front: it folds with it, or it would stick out
    for (size_t part : {dashboardGlowPart - 1, dashboardGlowPart})
      parts[part].model = parts[part].model->deformed(crumple);
  updateMirrorParts(); // (the glass is gone)
  if (hasAlarm) // the lamp is in the dashboard, which folds with the front
    parts[alarmPart].model = parts[alarmPart].model->deformed(crumple);
  // the fire alarm goes off
  onFire = true;
  alarmTime = 0.0f;
  if (soundEngine) {
    alarmSound = soundEngine->play(alarmClip(), true, position, true);
    if (alarmSound) {
      alarmSound->setVolume(ALARM_VOLUME);
      alarmSound->setDistances(4.0f, 60.0f);
    }
  }
  fuse = uniform(FUSE_MIN, FUSE_MAX); // (evenly distributed)
  // the engine stops now, and the key does nothing any more (see setEngine)
  setEngine(false);
  parkedLights = false;
  updateLights();
}

// Every part but the shell becomes a prop of its own, thrown away from the blast
void RV::ejectParts(const vec3 &from) {
  std::vector<size_t> list;
  if (hasWheels)
    for (int i = 0; i < 4; i++)
      list.push_back(wheelParts[i]);
  if (hasCockpit)
    for (size_t part : {dashboardGlowPart - 1, keyPart, speedNeedlePart, fuelNeedlePart})
      list.push_back(part);
  if (hasSteeringWheel)
    list.push_back(steeringWheelPart);
  if (hasWindshield)
    list.push_back(brokenWindshieldPart);
  if (hasHandbrake) {
    list.push_back(handbrakeBasePart);
    list.push_back(handbrakeLeverPart);
  }
  mat4 object = glm::translate(mat4(1.0f), position) * rotation;
  vec3 carried = getVelocity();
  for (size_t part : list) {
    Debris d;
    d.part = part;
    d.world = object * parts[part].local;
    vec3 low(1e9f), high(-1e9f);
    for (const Mesh &mesh : parts[part].model->meshes)
      for (const Vertex &v : mesh.getVertices()) {
        low = glm::min(low, v.Position);
        high = glm::max(high, v.Position);
      }
    if (low.x > high.x)
      continue; // (an empty model)
    d.centre = (low + high) * 0.5f;
    vec3 size = high - low;
    d.radius = 0.5f * std::min(size.x, std::min(size.y, size.z)) + 0.05f;
    vec3 at = vec3(d.world * vec4(d.centre, 1.0f));
    vec3 out = at - from;
    out = length(out) > 1e-3f ? normalize(out) : vec3(0.0f, 1.0f, 0.0f);
    float heavy = std::max(size.x, std::max(size.y, size.z)) > 1.5f ? 0.6f : 1.0f; // (the big ones are slower)
    d.velocity = carried + (out + vec3(0.0f, 0.9f, 0.0f)) * uniform(5.0f, 12.0f) * heavy;
    d.axis = normalize(vec3(uniform(-1.0f, 1.0f), uniform(-1.0f, 1.0f), uniform(-1.0f, 1.0f)) + vec3(0.0f, 0.01f, 0.0f));
    d.spin = uniform(4.0f, 14.0f);
    d.resting = false;
    debris.push_back(d);
  }
}

// The pieces fly, spin, fall and bounce on the floor until they lie still (like the mosquito's)
void RV::updateDebris(const Stage &stage, double dt) {
  float dtf = (float)dt;
  mat4 toObject = glm::inverse(glm::translate(mat4(1.0f), position) * rotation);
  for (Debris &d : debris) {
    if (!d.resting) {
      d.velocity.y -= 20.0f * dtf;
      vec3 c = vec3(d.world * vec4(d.centre, 1.0f));
      vec3 moved = c + d.velocity * dtf;
      mat4 turn = glm::translate(mat4(1.0f), moved) * glm::rotate(mat4(1.0f), d.spin * dtf, d.axis) *
                  glm::translate(mat4(1.0f), -c);
      d.world = turn * d.world;
      float ground = moved.y - d.radius - 1.0f;
      float h;
      if (stage.floorAt(moved.x, moved.z, h))
        ground = h;
      if (moved.y - d.radius < ground) { // a bounce, losing most of it
        d.world = glm::translate(mat4(1.0f), vec3(0.0f, ground + d.radius - moved.y, 0.0f)) * d.world;
        d.velocity = vec3(d.velocity.x * 0.5f, -d.velocity.y * 0.3f, d.velocity.z * 0.5f);
        d.spin *= 0.5f;
        if (length(d.velocity) < 0.8f)
          d.resting = true;
      }
    }
    setPartTransform(d.part, toObject * d.world);
  }
}

vec3 RV::engineBay() const {
  vec3 local = crumple(ENGINE_BAY);
  return position + vec3(rotation * vec4(local, 0.0f));
}

// The fire follows the front of the vehicle; the fuse runs down and the engine blows up
void RV::updateFire(double dt) {
  flashTime = std::max(0.0f, flashTime - (float)dt);
  if (!wrecked)
    return;
  vec3 at = engineBay();
  vec3 carried = getVelocity() * 0.8f;
  float big = exploded ? 2.0f : 1.0f;
  fire[0]->setPosition(at + vec3(0.0f, 0.3f, 0.0f));
  fire[0]->setBaseVelocity(carried);
  fire[0]->setRate(90.0f * big);
  fire[1]->setPosition(at + vec3(0.0f, 0.3f, 0.0f));
  fire[1]->setBaseVelocity(carried);
  fire[1]->setRate(30.0f * big);
  if (onFire && !exploded) { // the alarm: the lamp blinks and the beep follows the dashboard
    alarmTime += (float)dt;
    if (hasAlarm)
      setPartVisible(alarmPart, alarmLit());
    if (alarmSound)
      alarmSound->setPosition(position + vec3(rotation * vec4(crumple(ALARM_LAMP), 0.0f)));
  }
  if (!exploded) {
    fuse -= (float)dt;
    if (fuse <= 0.0f)
      explodeEngine();
  }
}

void RV::explodeEngine() {
  if (exploded)
    return;
  wreck();
  exploded = true;
  alarmSound.reset(); // (the dashboard is gone: no more alarm)
  if (hasAlarm)
    setPartVisible(alarmPart, false);
  vec3 centre = engineBay();
  vec3 carried = getVelocity() * 0.5f;
  const int counts[3] = {110, 60, 160};
  for (size_t i = 0; i < blast.size() && i < 3; i++) {
    blast[i]->setPosition(centre);
    blast[i]->setBaseVelocity(carried);
    blast[i]->burst(counts[i]);
  }
  flashTime = FLASH_TIME;
  // the engine is dead for good: no key, no fuel, no lights
  setFuel(0.0f);
  setEngine(false);
  setHeadlights(false);
  parkedLights = false;
  updateLights();
  ejectParts(centre);
  if (body) // the blast lifts the front
    body->setVelocity(body->getVelocity() + vec3(0.0f, 3.0f, 0.0f));
  if (soundEngine) {
    bangSound = soundEngine->play(explosionBangClip(), true, centre);
    if (bangSound)
      bangSound->setVolume(BANG_VOLUME);
  }
  if (explosionCallback)
    explosionCallback(centre, BLAST_RADIUS);
}

void RV::getFireLight(std::vector<SpotLight> &lights) const {
  vec3 at = engineBay() + vec3(0.0f, 0.5f, 0.0f);
  if (flashTime > 0.0f) {
    float k = flashTime / FLASH_TIME;
    lights.push_back(SpotLight::omni(at, vec3(1.0f, 0.6f, 0.2f) * (4.0f * k * k), 24.0f));
  }
  if (alarmLit()) // the red lamp lights the panel a little
    lights.push_back(SpotLight::omni(position + vec3(rotation * vec4(crumple(ALARM_LAMP) + vec3(0.0f, 0.0f, -0.1f), 0.0f)),
                                     vec3(1.0f, 0.05f, 0.03f) * 0.6f, 0.6f));
  if (!wrecked)
    return;
  float flicker = 1.6f + 0.5f * std::sin(23.0f * (float)time) + 0.3f * std::sin(37.0f * (float)time + 1.0f);
  lights.push_back(SpotLight::omni(at, vec3(1.0f, 0.45f, 0.12f) * (flicker * (exploded ? 1.5f : 1.0f)), 14.0f));
}

void RV::repairWindshield() {
  damagedWindshield = false;
  impact.clear();
  updateWindshieldParts();
}

void RV::updateWindshieldParts() {
  if (!hasWindshield)
    return;
  setPartVisible(windshieldPart, !damagedWindshield);
  setPartVisible(brokenWindshieldPart, damagedWindshield);
}

static VehicleBody::Surface surfaceOf(FloorMaterial material) {
  VehicleBody::Surface surface; // asphalt: as is
  if (material == FloorMaterial::Sand) {
    surface.grip = SAND_GRIP;
    surface.rolling = SAND_ROLLING;
    surface.topSpeed = SAND_TOP_SPEED;
  } else if (material == FloorMaterial::Grass) {
    surface.grip = GRASS_GRIP;
    surface.rolling = GRASS_ROLLING;
    surface.topSpeed = GRASS_TOP_SPEED;
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

void RV::setHeadlightGlowModel(std::shared_ptr<Model> model) {
  glowPart = addPart(model, 2); // emissive
  hasGlow = true;
  setPartVisible(glowPart, lampsLit());
}

void RV::setHandbrakeModels(std::shared_ptr<Model> base, std::shared_ptr<Model> lever) {
  handbrakeBasePart = addPart(base);
  handbrakeLeverPart = addPart(lever);
  hasHandbrake = true;
  setPartTransform(handbrakeBasePart, glm::translate(mat4(1.0f), HANDBRAKE_PIVOT));
  handbrakeAngle = radians(handbrakeOn ? HANDBRAKE_ENGAGED : HANDBRAKE_RELEASED);
  updateHandbrake(0.0);
}

// The lever swings towards where the switch says (pulled up = more upright; down = leaning forward)
void RV::updateHandbrake(double dt) {
  if (!hasHandbrake || exploded)
    return;
  float target = radians(handbrakeOn ? HANDBRAKE_ENGAGED : HANDBRAKE_RELEASED);
  float step = HANDBRAKE_SWING * (float)dt;
  handbrakeAngle += glm::clamp(target - handbrakeAngle, -step, step);
  mat4 lever = glm::translate(mat4(1.0f), HANDBRAKE_PIVOT);
  setPartTransform(handbrakeLeverPart, glm::rotate(lever, handbrakeAngle, vec3(1.0f, 0.0f, 0.0f)));
}

// The alarm's beep: a harsh tone for the first half of ALARM_PERIOD, silence for the rest (it is
// played in a loop); made once
static std::shared_ptr<AudioClip> alarmClip() {
  static std::shared_ptr<AudioClip> clip;
  if (clip)
    return clip;
  clip = std::make_shared<AudioClip>();
  clip->channels = 1;
  clip->sampleRate = 44100;
  const int n = (int)(44100 * ALARM_PERIOD);
  clip->samples.assign(n, 0.0f);
  for (int i = 0; i < n / 2; i++) {
    float t = i / 44100.0f, half = ALARM_PERIOD / 2.0f;
    float envelope = std::min(1.0f, std::min(t / 0.005f, (half - t) / 0.01f));
    float phase = 2.0f * 3.14159265f * 1500.0f * t;
    float tone = std::sin(phase) + 0.33f * std::sin(3.0f * phase); // (a little square)
    clip->samples[i] = 0.5f * envelope * std::tanh(1.5f * tone);
  }
  return clip;
}

bool RV::alarmLit() const { return onFire && !exploded && std::fmod(alarmTime, ALARM_PERIOD) < ALARM_PERIOD / 2.0f; }

void RV::setAlarmLampModel(std::shared_ptr<Model> lamp) {
  alarmPart = addPart(lamp, 2); // emissive
  hasAlarm = true;
  setPartVisible(alarmPart, false);
}

void RV::setMirrorTexture(int side, unsigned int texture, float aspect) {
  mirrorAspect = aspect;
  mat3 turn = mirrorTurn(side);
  vec3 across = turn * vec3(1.0f, 0.0f, 0.0f), up = turn * vec3(0.0f, 1.0f, 0.0f);
  vec3 normal = turn * vec3(0.0f, 0.0f, -1.0f); // (out of the face, towards the driver)
  vec3 centre = mirrorCentre(side) + normal * (MIRROR_DEPTH + MIRROR_PROUD);
  // seen from the driver, +across is on the left; the picture is shown mirrored (u = 1 at the left)
  float hw = MIRROR_HALF.x, hh = MIRROR_HALF.y;
  std::vector<Vertex> vertices = {
      {centre + across * hw - up * hh, normal, vec2(1.0f, 0.0f)},
      {centre - across * hw - up * hh, normal, vec2(0.0f, 0.0f)},
      {centre - across * hw + up * hh, normal, vec2(0.0f, 1.0f)},
      {centre + across * hw + up * hh, normal, vec2(1.0f, 1.0f)}};
  std::vector<unsigned int> indices = {0, 1, 2, 0, 2, 3};
  Texture picture;
  picture.id = texture;
  picture.type = "texture_diffuse";
  picture.path = "mirror";
  auto model = std::make_shared<Model>();
  model->meshes.push_back(Mesh(vertices, indices, {picture}));
  mirrorPart[side] = addPart(model, 2); // emissive: it is a picture, lighting does not touch it
  hasMirror[side] = true;
  updateMirrorParts();
}

bool RV::rearMirror(int side, MirrorView &view) const {
  if (side < 0 || side > 1 || !hasMirror[side] || wrecked)
    return false;
  mat3 body(rotation), turn = mirrorTurn(side);
  vec3 normal = body * (turn * vec3(0.0f, 0.0f, -1.0f));
  vec3 glass = position + body * (mirrorCentre(side) + turn * vec3(0.0f, 0.0f, -(MIRROR_DEPTH + MIRROR_PROUD)));
  vec3 incoming = glass - eyePosition();
  incoming = length(incoming) > 1e-3f ? normalize(incoming) : body * vec3(0.0f, 0.0f, 1.0f);
  view.forward = glm::reflect(incoming, normal); // where the driver sees in it
  view.position = glass + normal * 0.03f;
  view.up = body * vec3(0.0f, 1.0f, 0.0f);
  view.fov = MIRROR_FOV;
  view.aspect = mirrorAspect;
  return true;
}

void RV::setEngineSound(SoundEngine &sound) {
  soundEngine = &sound;
  engineSound.reset(new EngineSound(sound));
  // the starter is a recording; the running engine, the synth
  if (engineSound->setStartClip(START_SOUND))
    engineSim.setCrankTime((float)engineSound->getStartClipLength());
}

// Revs follow the road speed and the pedal; the sound follows the revs
void RV::updateEngineSound(double dt, float speed) {
  float pedal = (engineOn && occupied) ? std::fabs(throttle) : 0.0f;
  engineSim.update(dt, fuel > 0.0f, speed, pedal);
  if (keyOn && engineSim.hasGivenUp()) { // it would not start: the driver lets the key go
    keyOn = false;
    setEngineRunning(false);
  }
  // the engine has caught: from now on it can be used
  if (keyOn && !engineOn && engineSim.isRunning())
    setEngineRunning(true);
  if (engineSound)
    engineSound->update(position + vec3(0.0f, 1.0f, 0.0f), engineSim.getRpm(),
                        engineSim.getLoad(), engineSim.getStarter(), engineSim.getFire(),
                        engineSim.getPhase() == EngineSimulator::Phase::Cranking);
}

void RV::setEngineRunning(bool on) {
  engineOn = on;
  flickerTime = 0.0f; // a fault does not outlast a change of engine
  lampLevel = 1.0f;
  updateLights();
}

// The light switch keeps what the player chose: switching the engine off puts the lights out
// (lightsActive) but not the switch, so starting the engine again turns them back on
void RV::setEngine(bool on) {
  if (wrecked && on)
    return; // a burning engine can't be started again
  if (on == keyOn)
    return;
  keyOn = on;
  if (on) {
    engineSim.startAttempt(fuel > 0.0f, START_FAIL_CHANCE); // engineOn follows if it catches
  } else {
    engineSim.stop();
    setEngineRunning(false);
  }
}

void RV::setHeadlights(bool on) {
  if (on && !engineOn)
    return; // no electricity to switch them on without the engine
  headlightsOn = on;
  if (!on)
    parkedLights = false;
  flickerTime = 0.0f; // switching them ends any fault
  lampLevel = 1.0f;
  updateLights();
}

void RV::updateLights() {
  if (hasGlow)
    setPartVisible(glowPart, lampsLit());
  updateDashboardLights();
}

void RV::startLightFault() {
  if (!lightsActive() || flickerTime > 0.0f)
    return;
  flickerTime = uniform(FLICKER_MIN, FLICKER_MAX);
  flickerChange = 0.0f;
  faultGoesOut = uniform(0.0f, 100.0f) < lightOutChance;
}

// A fault starts by chance; while it lasts, the lamps jump between off, dim
// and bright at random short intervals, and at its end they come back or go out
void RV::updateHeadlights(double dt) {
  if (!lightsActive())
    return;
  if (flickerTime <= 0.0f) {
    // The chance per minute, as a chance for this frame (the same at any FPS)
    float perMinute = lightFaultChance / 100.0f;
    float now = perMinute >= 1.0f ? 1.0f
                                  : 1.0f - std::pow(1.0f - perMinute, (float)dt / 60.0f);
    if (uniform(0.0f, 1.0f) < now)
      startLightFault();
    if (flickerTime <= 0.0f)
      return;
  }
  flickerTime -= (float)dt;
  if (flickerTime <= 0.0f) {
    if (faultGoesOut)
      setHeadlights(false);
    else {
      flickerTime = 0.0f;
      lampLevel = 1.0f;
    }
  } else {
    flickerChange -= (float)dt;
    if (flickerChange <= 0.0f) {
      flickerChange = uniform(FLICKER_STEP_MIN, FLICKER_STEP_MAX);
      float pick = uniform(0.0f, 1.0f);
      lampLevel = pick < 0.5f ? 0.0f : pick < 0.8f ? uniform(0.15f, 0.4f) : uniform(0.7f, 1.0f);
    }
  }
  if (hasGlow)
    setPartVisible(glowPart, lampsLit());
  updateDashboardLights();
}

void RV::getHeadlights(std::vector<SpotLight> &lights) const {
  if (!lightsActive() || lampLevel <= 0.0f)
    return;
  vec3 aim = normalize(vec3(rotation * vec4(0.0f, HEADLIGHT_PITCH, 1.0f, 0.0f)));
  for (float side : {-1.0f, 1.0f}) {
    SpotLight light;
    light.position = position + vec3(rotation * vec4(side * HEADLIGHT_X, HEADLIGHT_Y,
                                                    HEADLIGHT_Z, 0.0f));
    light.direction = aim;
    light.color = vec3(1.1f, 1.0f, 0.82f) * lampLevel; // warm white
    lights.push_back(light);
  }
}

void RV::getDashboardLights(std::vector<SpotLight> &lights) const {
  if (!lightsActive() || lampLevel <= 0.0f || !hasCockpit)
    return;
  for (const DashLight &light : DASH_LIGHTS)
    lights.push_back(SpotLight::omni(position + vec3(rotation * vec4(light.position, 0.0f)),
                                     DASH_LIGHT_COLOR * lampLevel, light.range));
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

// A model's frame on the panel: its axes (x, y, z) and its origin, and optionally
// turned about its own z
static mat4 panelFrame(const vec3 &origin, float degrees) {
  mat4 frame(vec4(PANEL_X, 0.0f), vec4(PANEL_Y, 0.0f), vec4(PANEL_Z, 0.0f),
             vec4(origin, 1.0f));
  return glm::rotate(frame, radians(degrees), vec3(0.0f, 0.0f, 1.0f));
}

void RV::setCockpitModels(std::shared_ptr<Model> dashboard, std::shared_ptr<Model> key,
                          std::shared_ptr<Model> needle,
                          std::shared_ptr<Model> dashboardGlow) {
  addPart(dashboard); // in the RV's own frame: it needs no placing
  dashboardGlowPart = addPart(dashboardGlow, 2); // emissive, and in the same frame
  keyPart = addPart(key);
  speedNeedlePart = addPart(needle);
  fuelNeedlePart = addPart(needle);
  hasCockpit = true;
  updateDashboardLights();
  updateCockpit(0.0);
}

void RV::setSteeringWheelModel(std::shared_ptr<Model> wheel) {
  steeringWheelPart = addPart(wheel);
  hasSteeringWheel = true;
  placeSteeringWheel();
}

// The wheel is turned about its own z by the front wheels' steer angle (positive = left,
// counterclockwise seen from the driver)
void RV::placeSteeringWheel() {
  if (!hasSteeringWheel)
    return;
  float steer = body ? body->getWheels()[0].steer : 0.0f;
  mat4 frame(vec4(WHEEL_X, 0.0f), vec4(WHEEL_Y, 0.0f), vec4(WHEEL_Z, 0.0f), vec4(WHEEL_ORIGIN, 1.0f));
  setPartTransform(steeringWheelPart, glm::rotate(frame, steer * STEERING_RATIO, vec3(0.0f, 0.0f, 1.0f)));
}

// With the headlights on the dashboard lights up: its lit marks, lamps and display are
// shown, and the needles glow
void RV::updateDashboardLights() {
  if (!hasCockpit)
    return;
  setPartVisible(dashboardGlowPart, lampsLit());
  setPartUnlit(speedNeedlePart, lampsLit() ? 2 : 0);
  setPartUnlit(fuelNeedlePart, lampsLit() ? 2 : 0);
}

// The key and the needles follow the vehicle: a little smoothed, so that they move
// like real ones and not like numbers
void RV::updateCockpit(double dt) {
  if (!hasCockpit)
    return;
  float follow = 1.0f;
  if (dt > 0.0)
    follow = glm::min(1.0f, (float)dt * NEEDLE_RATE);
  // With the engine off the needles fall to empty, whatever the real speed and fuel
  float speed = body && engineOn ? std::fabs(body->getForwardSpeed()) : 0.0f;
  speedShown += (glm::clamp(speed / SPEEDOMETER_MAX, 0.0f, 1.0f) - speedShown) * follow;
  fuelShown += ((engineOn ? fuel : 0.0f) - fuelShown) * follow;
  // (the key goes a bit further while the starter is engaged, and springs back)
  keyTurn += ((keyOn ? (engineSim.getStarter() > 0.5f ? 1.3f : 1.0f) : 0.0f) - keyTurn) *
             (dt > 0.0 ? glm::min(1.0f, (float)dt * KEY_RATE) : 1.0f);

  setPartTransform(keyPart, panelFrame(KEY_ORIGIN, KEY_ON_ANGLE * keyTurn));
  setPartTransform(speedNeedlePart,
                   panelFrame(SPEED_NEEDLE_ORIGIN,
                              SPEED_ANGLES[0] + (SPEED_ANGLES[1] - SPEED_ANGLES[0]) * speedShown));
  setPartTransform(fuelNeedlePart,
                   panelFrame(FUEL_NEEDLE_ORIGIN,
                              FUEL_ANGLES[0] + (FUEL_ANGLES[1] - FUEL_ANGLES[0]) * fuelShown));
}

vec3 RV::eyePosition() const {
  return position + vec3(rotation * vec4(EYE_X, EYE_Y, EYE_Z, 0.0f));
}

void RV::attachCamera(Camera *camera, float distance, float height) {
  this->camera = camera;
  chaseDistance = distance;
  chaseHeight = height;
  setYaw(facing);
  applyCameraView();
}

void RV::applyCameraView() {
  if (!camera)
    return;
  if (cameraView == CameraView::Chase) {
    camera->attachTo(this, chaseDistance, chaseHeight);
    // (world angles again: look along the heading)
    camera->setAngles(headingYaw(), 0.0f);
  } else {
    camera->attachTo(nullptr, 0.0f, 0.0f); // nothing to orbit: followCamera places it
    camera->setAngles(0.0f, 0.0f);         // (relative to the vehicle: straight ahead)
    followCamera();
  }
}

void RV::setCameraView(CameraView view) {
  cameraView = view;
  applyCameraView();
}

void RV::followCamera() {
  if (!camera)
    return;
  if (cameraView == CameraView::Chase) {
    camera->follow();
    return;
  }
  // Cockpit: the view turns, pitches and rolls with the vehicle, as if the head
  // were in it (the mouse looks around from there: the camera's angles are
  // relative to the vehicle)
  // The vehicle's heading, pitch (nose up = positive) and roll
  vec3 forward = vec3(rotation * vec4(0.0f, 0.0f, 1.0f, 0.0f));
  vec3 up = vec3(rotation * vec4(0.0f, 1.0f, 0.0f, 0.0f));
  float yaw = std::atan2(forward.x, forward.z);
  float pitch = std::asin(glm::clamp(forward.y, -1.0f, 1.0f));
  mat3 level = mat3(glm::rotate(mat4(1.0f), yaw, vec3(0.0f, 1.0f, 0.0f))) *
               mat3(glm::rotate(mat4(1.0f), -pitch, vec3(1.0f, 0.0f, 0.0f)));
  vec3 flatUp = level * vec3(0.0f, 1.0f, 0.0f); // up with no roll
  float roll = std::atan2(dot(cross(flatUp, up), forward), dot(flatUp, up));
  mat3 carrier = level * mat3(glm::rotate(mat4(1.0f), roll * COCKPIT_ROLL,
                                          vec3(0.0f, 0.0f, 1.0f)));
  // (the camera looks towards -z at yaw 0 and the RV's front is +z: half a turn)
  camera->setCarrier(carrier *
                     mat3(glm::rotate(mat4(1.0f), 3.14159265f, vec3(0.0f, 1.0f, 0.0f))));
  vec3 eye = eyePosition();
  camera->reposition(eye.x, eye.y, eye.z);
}

void RV::control(vec2 dir, float up, float cameraYaw) {
  throttle = -dir.y; // W is forward
  steering = dir.x;  // D is right
}

void RV::update(double dt) {
  // Only the input is read here: the stage moves the RV through contactFloor()
  GameObject::update(dt);
  updateCockpit(dt);
  placeSteeringWheel();
  updateHeadlights(dt);
  updateHandbrake(dt);
  updateFire(dt);
  if (wrecked && !exploded) // the small things on the dashboard go where the crushed dashboard is
    for (size_t part : {keyPart, speedNeedlePart, fuelNeedlePart, steeringWheelPart}) {
      if (!hasCockpit || (part == steeringWheelPart && !hasSteeringWheel))
        continue;
      vec3 at = vec3(parts[part].local[3]);
      parts[part].local = glm::translate(mat4(1.0f), crumple(at) - at) * parts[part].local;
    }
}

// Wheel i hangs from its anchor at the length the suspension has now
void RV::placeWheels() {
  if (!hasWheels || exploded) // (after the explosion the wheels are props of their own)
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
    if (flatTires[i]) // squashed down to the smaller radius it rolls on
      local = glm::scale(local, vec3(1.0f, (WHEEL_RADIUS - p.flatDrop) / WHEEL_RADIUS, 1.0f));
    setPartTransform(wheelParts[i], local);
  }
}

vec3 RV::wheelHub(int wheel) const {
  const VehicleBody::Params &p = body ? body->getParams() : vehicleParams(0, 0);
  int i = glm::clamp(wheel, 0, 3);
  float length = body ? body->getWheels()[i].length : p.restLength;
  const vec3 &anchor = p.wheels[i].anchor;
  return position + vec3(rotation * vec4(anchor.x, anchor.y - length, anchor.z, 0.0f));
}

void RV::punctureTire(int wheel) {
  if (wheel < 0 || wheel > 3)
    return;
  flatTires[wheel] = true;
  if (body)
    body->setFlat(wheel, true);
  placeWheels();
}

void RV::repairTires() {
  for (int i = 0; i < 4; i++) {
    flatTires[i] = false;
    if (body)
      body->setFlat(i, false);
  }
  placeWheels();
}

// The cap of the tank: on the -x side (the door is on +x), above and behind the rear wheel
static const vec3 FUEL_CAP(-1.2f, 1.25f, -3.05f);

void RV::fuelCap(vec3 &where, vec3 &normal) const {
  where = position + vec3(rotation * vec4(FUEL_CAP, 0.0f));
  normal = vec3(rotation * vec4(-1.0f, 0.0f, 0.0f, 0.0f));
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
    ParticleEmitter &grain = *grains[i];
    if (!onSand || speed < DUST_MIN_SPEED) {
      emitter.setRate(0.0f);
      grain.setRate(0.0f);
      continue;
    }
    grain.setPosition(contact + vec3(0.0f, 0.05f, 0.0f));
    grain.setDirection(away * GRAIN_BACKWARDS + vec3(0.0f, GRAIN_UPWARDS, 0.0f));
    grain.setBaseVelocity(velocity * GRAIN_INHERIT);
    grain.setRate(GRAIN_RATE * std::min(speed, DUST_MAX_RATE_SPEED) / 10.0f);
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
    for (int i = 0; i < 4; i++)
      body->setFlat(i, flatTires[i]);
  }
  // What each wheel drives on, from the stage's floor
  body->setSurfaceQuery([&stage](float x, float z) {
    return surfaceOf(stage.materialAt(x, z));
  });
  // No fuel: the engine does not push (it can steer and coast, and the brake holds it
  // when it is nearly stopped)
  bool noEngine = fuel <= 0.0f || !engineOn; // no fuel, or the engine is switched off
  // Only the lever brakes: getting out does not set it, and neither does an engine that is off
  body->setHandbrake(handbrakeOn);
  body->setInput(noEngine ? 0.0f : throttle, -steering);
  body->step(dt, [&stage](float x, float z, float maxY, float &height,
                          vec3 &normal) {
    return stage.floorAt(x, z, height, &normal, maxY);
  });

  updateEngineSound(dt, body->getForwardSpeed());

  // Driving burns fuel in proportion to the speed (only while somebody drives it)
  if (occupied && engineOn && fuel > 0.0f)
    setFuel(fuel - fuelPerMeter * std::fabs(body->getForwardSpeed()) * (float)dt);

  // The timer of a frontal collision follows the speed (after the physics of this frame)
  impact.update((float)dt, body->getForwardSpeed());
  handleImpact();

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
  updateDebris(stage, dt);
  return true;
}

void RV::describe(std::vector<std::string> &lines) const {
  PlayableCharacter::describe(lines);
  if (hasWindshield)
    lines.push_back(std::string("Parabrisas: ") + (damagedWindshield ? "ROTO" : "intacto") +
                    (impact.timerRunning() ? "  (cronometro de choque en marcha)" : ""));
  lines.push_back(std::string("En llamas (alarma): ") + (onFire ? "SI" : "no"));
  if (wrecked)
    lines.push_back(exploded ? "Motor: EXPLOTADO"
                             : textFormat("Motor: EN LLAMAS, explota en %.1f s", fuse));
  if (hasCockpit)
    lines.push_back(textFormat("Combustible: %.0f %%  Llave: %s", fuel * 100.0f,
                               engineOn ? "puesta, motor en marcha" : "puesta, motor apagado"));
  lines.push_back(textFormat("Faros: %s  Averia: %.1f %%/min, se apagan el %.0f %%",
                             !lightsActive() ? "apagados"
                             : flickerTime > 0.0f ? "parpadeando"
                                                  : "encendidos",
                             lightFaultChance, lightOutChance));
  lines.push_back(std::string("Camara: ") +
                  (cameraView == CameraView::Cockpit ? "cabina" : "exterior"));
  lines.push_back(std::string("Ocupado: ") + (occupied ? "si" : "no") +
                  textFormat("  Acelerador: %.1f  Volante: %.1f%s", throttle,
                             steering, handbrakeOn ? "  Freno de mano" : ""));
  if (!body)
    return; // no physics until the first update
  lines.push_back(textFormat("Vehiculo: %.2f m/s hacia delante",
                             tidy(body->getForwardSpeed())));
  lines.push_back("Vel. angular: " + textOf(body->getAngularVelocity()));
  lines.push_back("Centro de masas: " + textOf(body->getCentreOfMass()));
  // Suspension length of each wheel (* = touching the ground). Same order as
  // wheelParts; facing +z, -x is the right-hand side: DD = front right...
  std::string wheels = "Suspension (m):";
  const char *names[] = {"DD", "DI", "TD", "TI"};
  const std::vector<VehicleBody::WheelState> &states = body->getWheels();
  for (size_t i = 0; i < states.size(); i++)
    wheels += textFormat(" %s %.2f%s%s", i < 4 ? names[i] : "?", states[i].length,
                         states[i].onGround ? "*" : "", isTireFlat((int)i) ? " PINCHADA" : "");
  lines.push_back(wheels);
}

void RV::getProperties(std::vector<Property> &properties) {
  // Not DynamicGameObject's: the RV's motion is its body's
  GameObject::getProperties(properties);
  properties.push_back(Property::number(
      "Velocidad (hacia delante)", -10.0f, 30.0f, 0.0f,
      [this]() { return body ? body->getForwardSpeed() : 0.0f; },
      [this](float speed) {
        if (!body)
          return;
        vec3 forward = normalize(vec3(rotation[2]));
        body->setVelocity(body->getVelocity() +
                          forward * (speed - body->getForwardSpeed()));
      },
      "m/s"));
  properties.push_back(Property::number(
      "Velocidad maxima", 2.0f, 40.0f, 0.5f, [this]() { return maxSpeed; },
      [this](float speed) {
        maxSpeed = speed;
        if (body)
          body->setMaxSpeed(speed);
      },
      "m/s"));
  properties.push_back(Property::number(
      "Combustible", 0.0f, 100.0f, 1.0f, [this]() { return fuel * 100.0f; },
      [this](float percent) { setFuel(percent / 100.0f); }, "%"));
  properties.push_back(Property::toggle(
      "Motor encendido", [this]() { return engineOn; },
      [this](bool on) { setEngine(on); }));
  properties.push_back(Property::toggle(
      "Freno de mano", [this]() { return handbrakeOn; },
      [this](bool on) { setHandbrakeOn(on); }));
  properties.push_back(Property::toggle(
      "Faros encendidos", [this]() { return headlightsOn; },
      [this](bool on) { setHeadlights(on); }));
  properties.push_back(Property::number(
      "Faros: prob. de averia", 0.0f, 100.0f, 0.5f,
      [this]() { return lightFaultChance; },
      [this](float v) { setLightFaultChance(v); }, "%/min"));
  properties.push_back(Property::number(
      "Faros: prob. de apagarse", 0.0f, 100.0f, 1.0f,
      [this]() { return lightOutChance; },
      [this](float v) { setLightOutChance(v); }, "%"));
  properties.push_back(Property::info("Estado de los faros", [this]() {
    if (!lightsActive())
      return std::string("apagados");
    if (flickerTime > 0.0f)
      return std::string(faultGoesOut ? "parpadean (se apagaran)"
                                      : "parpadean (volveran)");
    return std::string("encendidos");
  }));
  properties.push_back(Property::action("Faros: provocar una averia",
                                        [this]() { startLightFault(); }));
  properties.push_back(Property::info("Ruedas pinchadas", [this]() {
    const char *names[] = {"DD", "DI", "TD", "TI"};
    std::string flat;
    for (int i = 0; i < 4; i++)
      if (flatTires[i])
        flat += std::string(flat.empty() ? "" : " ") + names[i];
    return flat.empty() ? std::string("ninguna") : flat;
  }));
  properties.push_back(Property::action("Pinchar una rueda al azar", [this]() {
    punctureTire(std::uniform_int_distribution<int>(0, 3)(random));
  }));
  properties.push_back(Property::action("Reparar las ruedas", [this]() { repairTires(); }));
  properties.push_back(Property::action("Estrellar de frente (incendio)", [this]() { wreck(); }));
  properties.push_back(Property::action("Explotar el motor", [this]() { explodeEngine(); }));
  if (hasWindshield)
    properties.push_back(Property::toggle(
        "Parabrisas roto", [this]() { return damagedWindshield; },
        [this](bool broken) {
          if (broken)
            breakWindshield();
          else
            repairWindshield();
        }));
}

void RV::teleport(const vec3 &position) {
  PlayableCharacter::teleport(position);
  vec3 forward = vec3(rotation[2]);
  if (length(vec2(forward.x, forward.z)) > 1e-4f)
    facing = std::atan2(forward.x, forward.z); // its heading now
  setYaw(facing); // upright
  if (body)
    body->place(position, facing);
  placeWheels();
}

void RV::turn(float radians) {
  PlayableCharacter::turn(radians);
  teleport(position); // upright at the new heading, body included
}
