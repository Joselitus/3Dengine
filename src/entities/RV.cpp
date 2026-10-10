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
// The door (generate_rv.py: HINGE and DOOR_HOLE): the hinge's place on the front edge of the doorway, how
// far it opens (radians, outwards) and how fast it swings (rad/s); the point to be near to use it
static const vec3 DOOR_HINGE(1.22f, 0.58f, 0.12f), DOOR_POINT(1.2f, 0.3f, -0.35f);
static const float DOOR_OPEN_ANGLE = 1.75f; // (100 degrees, outwards)
// The door as a swinging panel: its width (m, hinge to free edge), how fast the use key carries it open
// and shut (rad/s), the hinge's friction (1/s), how much it bounces off its stops, the
// speed below which it latches when it closes, and the push (m/s^2 across the panel) it takes to
// pull it off its open stop (a catch holds it there: a tilt or a gentle start does not shut it)
static const float DOOR_WIDTH = 0.94f, DOOR_HAND_SPEED = 2.5f;
static const float DOOR_DAMPING = 1.5f, DOOR_BOUNCE_OPEN = 0.25f, DOOR_BOUNCE_SHUT = 0.3f, DOOR_LATCH_SPEED = 2.0f;
static const float DOOR_CATCH = 3.0f;
// Where the driver stands on the floor inside (behind the wheel at x = 0.45, z = 2.28)
// (on the floor behind the pilot's seat, which stands at SEAT_ORIGIN: its cushion and backrest take
// z 1.16 to 1.94, its back leaning to z 0.95)
static const vec3 DRIVER_STAND(0.45f, 0.55f, 0.45f);
static const vec3 SEAT_ORIGIN(0.45f, 0.55f, 1.55f); // the pilot's seat's floor point; the copilot's is at -x
// A penguin sitting there: his hips over the cushion's top (generate_seat.py: 0.46 m over the floor,
// times its SCALE 1.45) at this z: the driver nearer its front edge (to reach the wheel)
static const float SEAT_CUSHION = 0.667f, SIT_Z_DRIVER = 1.5f, SIT_Z_PASSENGER = 1.5f;
// The steering wheel's rim (generate_steering_wheel.py: RIM_R)
static const float WHEEL_RIM_RADIUS = 0.27f;
// The wreck: a crash this many times harder than the least that breaks the windshield wrecks the
// front; the fuse of the explosion lasts between these (s); the blast reaches this far (m); and
// the engine bay is here, in the frame of rv.obj
static const float WRECK_SEVERITY = 2.0f;
static const float FUSE_MIN = 0.1f, FUSE_MAX = 30.0f;
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
// The lost-wheel lamp, the one beside the alarm's (written by generate_dashboard.py)
static const vec3 TIRE_LAMP(0.265f, 1.8572f, 2.759f);
// The sparks of a missing wheel: they start at this speed (m/s) and their number grows with it
static const float SPARK_MIN_SPEED = 1.0f, SPARK_RATE = 140.0f, SPARK_BACKWARDS = 0.7f, SPARK_UPWARDS = 0.5f,
                   SPARK_INHERIT = 0.25f;
static std::shared_ptr<AudioClip> alarmClip();
// The three mirrors (generate_rv.py, turned_box): 0 the driver's side mirror (+x), 1 the passenger's
// (-x), heads of 0.22 x 0.42 x 0.12; 2 the central one inside, at the top of the windshield
// (CENTRE_MIRROR there), a head of 0.28 x 0.09 x 0.04. Each head is turned as a whole MIRROR_YAW about
// the vertical and MIRROR_PITCH about its own x, angles that bounce the driver's line of sight
// straight back (the central one's, to the upper part of the rear window). The glass is a flat quad
// lying on the rear face of a head (MIRROR_DEPTH from its centre), MIRROR_HALF wide and tall, and
// the picture on it is the view of a camera at the glass: MIRROR_FOV degrees (vertical); the side
// ones show more than a flat mirror would, like convex ones.
static const vec3 MIRROR_CENTRE[RV::MIRRORS] = {vec3(1.51f, 2.01f, 3.38f), vec3(-1.51f, 2.01f, 3.38f),
                                                vec3(0.0f, 2.55f, 2.80f)};
static const float MIRROR_YAW[RV::MIRRORS] = {14.0f, -22.5f, -9.84f}; // degrees (signed)
static const float MIRROR_PITCH[RV::MIRRORS] = {4.8f, 3.7f, -5.29f};
static const float MIRROR_FOV[RV::MIRRORS] = {30.0f, 30.0f, 10.0f};
static const float MIRROR_DEPTH[RV::MIRRORS] = {0.06f, 0.06f, 0.02f}, MIRROR_PROUD = 0.004f;
static const vec2 MIRROR_HALF[RV::MIRRORS] = {vec2(0.095f, 0.19f), vec2(0.095f, 0.19f), vec2(0.13f, 0.04f)};
static vec3 mirrorCentre(int side) { return MIRROR_CENTRE[side]; }
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

// The collision shape: a HOLLOW box, a compound of boxes laid just outside the thin walls of rv.obj (so
// that what is inside the sheet is the cab's room): floor, roof, rear, the two sides (the +x one in
// pieces, with the doorway), a solid block for the dashboard and the engine under the windshield, the
// slanted windshield, the front fascia, the step under the door and the door itself (the only part
// that is switched off when it is open). Keep the measures the same as generate_rv.py's (PROF,
// DOOR_HOLE).
enum HullPart { HULL_DOOR = 5 }; // (the index of the door's box: keep it in step with hullShape)
static std::shared_ptr<CompoundShape> hullShape() {
  auto hull = std::make_shared<CompoundShape>();
  auto box = [&](float x0, float x1, float y0, float y1, float z0, float z1) {
    return hull->add(vec3(x1 - x0, y1 - y0, z1 - z0) * 0.5f, vec3(x0 + x1, y0 + y1, z0 + z1) * 0.5f);
  };
  const float W = HALF_TRACK, T = 0.1f, LOW = 0.5f, FLOOR = 0.55f, ROOF = 3.05f;
  box(-W, W, LOW, FLOOR, -3.55f, 3.55f);                 // 0 the floor (its top is the sheet of rv.obj)
  box(-W - T, W + T, ROOF, ROOF + T, -3.55f - T, 3.0f);  // 1 the roof
  box(-W - T, W + T, LOW, ROOF + T, -3.55f - T, -3.55f); // 2 the rear wall
  box(-W - T, -W, LOW, ROOF + T, -3.55f - T, 3.55f);     // 3 the -x wall
  box(W, W + T, LOW, ROOF + T, 0.1f, 3.55f);             // 4 the +x wall, in front of the doorway...
  box(W, W + T, 0.6f, 2.45f, -0.8f, 0.1f);               // 5 the door, shut (HULL_DOOR)
  box(W, W + T, LOW, ROOF + T, -3.55f - T, -0.8f);       // 6 ...and behind it
  box(W, W + T, 2.45f, ROOF + T, -0.8f, 0.1f);           // 7 over the doorway
  box(W, W + T, LOW, 0.6f, -0.8f, 0.1f);                 // 8 the sill
  box(-W, W, FLOOR, 1.8f, 2.55f, 3.55f);                 // 9 the dashboard and what is under it
  box(-1.25f, 1.25f, LOW, 1.7f, 3.55f, 3.75f);           // 10 the front fascia and bumper
  hull->setFloorContact(box(W, W + 0.3f, 0.25f, 0.34f, -0.75f, 0.05f), false); // 11 the step under the door (it hangs low: it does not count for the floor)
  // 12 the windshield, a thin slab along its slope (from z 3.55, y 1.7 to z 3.0, y 2.95), turned about x
  const float nz = 1.25f / 1.3658f, ny = 0.55f / 1.3658f; // (the slope's direction up is (-ny, nz) in (z, y))
  mat3 slope(vec3(1.0f, 0.0f, 0.0f), vec3(0.0f, nz, -ny), vec3(0.0f, ny, nz));
  hull->add(vec3(W, 0.683f, 0.05f), vec3(0.0f, 2.325f + 0.05f * ny, 3.275f + 0.05f * nz), slope);
  // 13, 14 the seats (pilot's, copilot's), solid from the floor up to over the backrest's lean: too tall
  // to be stepped on, so that one walks round them (behind them) and sits with E
  for (float side : {1.0f, -1.0f})
    box(side * SEAT_ORIGIN.x - 0.42f, side * SEAT_ORIGIN.x + 0.42f, FLOOR, 2.15f, 0.95f, 1.97f);
  return hull;
}
void RV::interiorBox(vec3 &centre, vec3 &halfSize) {
  // (as hullShape: from a little under the floor, so that feet on it count, up to the roof)
  const float W = HALF_TRACK, BOTTOM = 0.3f, ROOF = 3.05f, BACK = -3.55f, FRONT = 3.55f;
  centre = vec3(0.0f, (BOTTOM + ROOF) * 0.5f, (BACK + FRONT) * 0.5f);
  halfSize = vec3(W, (ROOF - BOTTOM) * 0.5f, (FRONT - BACK) * 0.5f);
}

// Where the springy contact points are: 4 cm outside that box
static const float SKIN = 0.04f;
static const float CHASSIS_X = CHASSIS_HALF_X + SKIN;
static const float CHASSIS_Z = CHASSIS_HALF_Z + SKIN;
static const float CHASSIS_BOTTOM = CHASSIS_LOW - SKIN;
static const float CHASSIS_TOP = CHASSIS_HIGH + SKIN;

RV::RV(std::shared_ptr<Model> model) : RV(model, hullShape()) {}

RV::RV(std::shared_ptr<Model> model, std::shared_ptr<CompoundShape> hull)
    : PlayableCharacter(model, hull), random(std::random_device()()), hull(hull) {
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

  ParticleSettings spark; // friction sparks: small, yellow, bright, thrown back, they fall in a trace
  spark.lifeMin = 0.35f;
  spark.lifeMax = 0.8f;
  spark.speedMin = 2.0f;
  spark.speedMax = 6.0f;
  spark.spread = 0.6f;
  spark.sizeStart = 0.07f;
  spark.sizeEnd = 0.03f;
  spark.color = vec3(1.0f, 0.85f, 0.25f);
  spark.alpha = 1.0f;
  spark.fadeStart = 0.5f;
  spark.gravity = 12.0f;
  spark.drag = 0.4f;
  spark.maxParticles = 250;
  for (unsigned i = 0; i < 4; i++)
    sparks.push_back(std::make_shared<ParticleEmitter>(spark, 300 + i));

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
// It kills whoever it hits on foot when it goes faster than this (m/s)
static const float RUN_OVER_SPEED = 3.0f;

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

bool RV::isRunningOver(const vec3 &feet) const {
  if (wrecked || length(getVelocity()) < RUN_OVER_SPEED)
    return false;
  vec3 local = vec3(transpose(mat3(rotation)) * (feet - position));
  // (a little wider than the body, so that its side hits as well; from the bottom of the body up
  // to its roof)
  return std::fabs(local.x) < BODY_HALF_WIDTH + 0.3f && std::fabs(local.z) < FRONT_Z + 0.3f &&
         local.y > -0.5f && local.y < 2.5f;
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
  if (hasTireLamp)
    parts[tireLampPart].model = parts[tireLampPart].model->deformed(crumple);
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
      if (!flatTires[i]) // (a wheel that is gone already is a prop of its own)
        list.push_back(wheelParts[i]);
  if (hasCockpit)
    for (size_t part : {dashboardGlowPart - 1, keyPart, speedNeedlePart, fuelNeedlePart})
      list.push_back(part);
  if (hasSteeringWheel)
    list.push_back(steeringWheelPart);
  if (hasWindshield)
    list.push_back(brokenWindshieldPart);
  if (hasDoor)
    list.push_back(doorPart);
  if (hasSeats) {
    list.push_back(seatParts[0]);
    list.push_back(seatParts[1]);
  }
  if (hasHandbrake) {
    list.push_back(handbrakeBasePart);
    list.push_back(handbrakeLeverPart);
  }
  for (size_t part : list)
    launchPart(part, from, 5.0f, 12.0f);
}

void RV::launchPart(size_t part, const vec3 &from, float lowest, float highest) {
  mat4 object = glm::translate(mat4(1.0f), position) * rotation;
  vec3 carried = getVelocity();
  {
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
      return; // (an empty model)
    d.centre = (low + high) * 0.5f;
    vec3 size = high - low;
    d.radius = 0.5f * std::min(size.x, std::min(size.y, size.z)) + 0.05f;
    vec3 at = vec3(d.world * vec4(d.centre, 1.0f));
    vec3 out = at - from;
    out = length(out) > 1e-3f ? normalize(out) : vec3(0.0f, 1.0f, 0.0f);
    float heavy = std::max(size.x, std::max(size.y, size.z)) > 1.5f ? 0.6f : 1.0f; // (the big ones are slower)
    d.velocity = carried + (out + vec3(0.0f, 0.9f, 0.0f)) * uniform(lowest, highest) * heavy;
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
  if (!exploded && !replica) { // (a copy blows up when the server says so)
    fuse -= (float)dt;
    if (fuse <= 0.0f)
      explodeEngine();
  }
}

void RV::takeDamage(float amount, const vec3 &direction, const Stage &stage) {
  if (!replica)
    explodeEngine();
}

void RV::explodeEngine() {
  if (exploded)
    return;
  wreck();
  exploded = true;
  hull->setEnabled(HULL_DOOR, false); // (the door is blown off: the doorway is open)
  alarmSound.reset(); // (the dashboard is gone: no more alarm)
  if (hasAlarm)
    setPartVisible(alarmPart, false);
  if (hasTireLamp)
    setPartVisible(tireLampPart, false);
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
  if (body) {
    // The wheels are gone: the chassis is a rigid box now. The blast lifts it and sets it
    // tumbling (about its length and sideways), and then it falls, bounces and settles by itself
    body->setWrecked(true);
    body->setVelocity(body->getVelocity() + vec3(0.0f, 7.0f, 0.0f));
    body->setAngularVelocity(body->getAngularVelocity() + vec3(-uniform(0.5f, 3.0f), uniform(-0.8f, 0.8f), uniform(-6.0f, 6.0f)));
  }
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
  for (int i = 0; i < 4; i++) // the sparks of a missing wheel light the road a little
    if (sparkLit[i])
      lights.push_back(SpotLight::omni(sparkSpot[i] + vec3(0.0f, 0.15f, 0.0f), vec3(1.0f, 0.7f, 0.2f) * 1.5f, 5.0f));
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
  if (side < 0 || side >= MIRRORS)
    return;
  mirrorAspect[side] = aspect;
  mat3 turn = mirrorTurn(side);
  vec3 across = turn * vec3(1.0f, 0.0f, 0.0f), up = turn * vec3(0.0f, 1.0f, 0.0f);
  vec3 normal = turn * vec3(0.0f, 0.0f, -1.0f); // (out of the face, towards the driver)
  vec3 centre = mirrorCentre(side) + normal * (MIRROR_DEPTH[side] + MIRROR_PROUD);
  // seen from the driver, +across is on the left; the picture is shown mirrored (u = 1 at the left)
  float hw = MIRROR_HALF[side].x, hh = MIRROR_HALF[side].y;
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
  if (side < 0 || side >= MIRRORS || !hasMirror[side] || wrecked)
    return false;
  mat3 body(rotation), turn = mirrorTurn(side);
  vec3 normal = body * (turn * vec3(0.0f, 0.0f, -1.0f));
  vec3 glass = position + body * (mirrorCentre(side) + turn * vec3(0.0f, 0.0f, -(MIRROR_DEPTH[side] + MIRROR_PROUD)));
  vec3 incoming = glass - eyePosition();
  incoming = length(incoming) > 1e-3f ? normalize(incoming) : body * vec3(0.0f, 0.0f, 1.0f);
  view.forward = glm::reflect(incoming, normal); // where the driver sees in it
  view.position = glass + normal * 0.03f;
  view.up = body * vec3(0.0f, 1.0f, 0.0f);
  view.fov = MIRROR_FOV[side];
  view.aspect = mirrorAspect[side];
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

// Using the door only opens or closes it: to drive, the penguin gets in and uses the steering wheel
void RV::onUse(const vec3 &playerPosition) { toggleDoor(); }

vec3 RV::getInteractionPoint() const { return position + vec3(rotation * vec4(DOOR_POINT, 0.0f)); }

vec3 RV::driverStand() const { return position + vec3(rotation * vec4(DRIVER_STAND, 0.0f)); }

void RV::setSeatModel(std::shared_ptr<Model> seat) {
  for (int i = 0; i < 2; i++) {
    seatParts[i] = addPart(seat);
    setPartTransform(seatParts[i], glm::translate(mat4(1.0f), vec3(i == 0 ? SEAT_ORIGIN.x : -SEAT_ORIGIN.x,
                                                                    SEAT_ORIGIN.y, SEAT_ORIGIN.z)));
  }
  hasSeats = true;
}

vec3 RV::copilotStand() const {
  return position + vec3(rotation * vec4(-DRIVER_STAND.x, DRIVER_STAND.y, DRIVER_STAND.z, 0.0f));
}

// His feet are put so that his eyes (1.6 m over them) are the pilot's, on the -x side
vec3 RV::copilotSeatPosition() const {
  return position + vec3(rotation * vec4(-EYE_X, EYE_Y - 1.6f, EYE_Z, 0.0f));
}

vec3 RV::sittingPoint(bool copilot) {
  return vec3(copilot ? -SEAT_ORIGIN.x : SEAT_ORIGIN.x, SEAT_ORIGIN.y + SEAT_CUSHION,
              copilot ? SIT_Z_PASSENGER : SIT_Z_DRIVER);
}

float RV::steeringWheelAngle() const { return (body ? body->getWheels()[0].steer : 0.0f) * STEERING_RATIO; }

vec3 RV::steeringRim(float angle) const {
  float a = angle + steeringWheelAngle();
  return WHEEL_ORIGIN + WHEEL_RIM_RADIUS * (std::cos(a) * WHEEL_X + std::sin(a) * WHEEL_Y);
}

void RV::setDoorModel(std::shared_ptr<Model> door) {
  doorPart = addPart(door);
  hasDoor = true;
  updateDoor(0.0);
}

// A hand carries it all the way: open to its stop if it is shut or closing, else shut until it latches
// (a push alone used to leave it ajar: the hinge's friction or a tilt stopped it half way)
void RV::toggleDoor() {
  if (doorLatched) {
    doorLatched = false;
    doorAngle = 0.02f;
    doorHand = 1;
  } else {
    doorHand = doorHand > 0 || doorAngle >= DOOR_OPEN_ANGLE - 0.05f ? -1 : 1;
  }
}

// The door is a panel hanging on a vertical hinge: in the vehicle's frame it feels a push of
// g - a (gravity, only along the door's plane when the RV is tilted, and minus the acceleration
// of the point where it is, which counts the vehicle's spin too), and swings under it, with
// the hinge's friction, bouncing off its two stops. About a vertical hinge a uniform panel of width L
// at an angle t (0 = shut, its free edge towards the rear) has t'' = 3/(2L) (cos t fx + sin t fz), f being
// that push per kilo, in the vehicle's x and z. So braking or turning hard throws it open, accelerating
// slams it shut, and a gentle close latches it.
void RV::updateDoor(double dt) {
  if (!hasDoor || exploded)
    return;
  float dtf = (float)dt;
  vec3 v = velocity, w = body ? body->getAngularVelocity() : vec3(0.0f);
  if (dtf > 1e-5f) { // (the vehicle's acceleration and angular acceleration, smoothed over a few frames)
    float k = std::min(1.0f, 25.0f * dtf);
    doorAccel += ((v - doorPrevVelocity) / dtf - doorAccel) * k;
    doorAlpha += ((w - doorPrevSpin) / dtf - doorAlpha) * k;
  }
  doorPrevVelocity = v;
  doorPrevSpin = w;
  if (!doorLatched && dtf > 0.0f) {
    mat3 R(rotation);
    float c = 0.5f * DOOR_WIDTH;
    // the middle of the panel, in the vehicle's frame, relative to its centre of mass (0.4 up)
    vec3 middle = DOOR_HINGE + vec3(c * std::sin(doorAngle), 0.0f, -c * std::cos(doorAngle));
    vec3 r = R * (middle - vec3(0.0f, 0.4f, 0.0f));
    vec3 a = doorAccel + glm::cross(doorAlpha, r) + glm::cross(w, glm::cross(w, r));
    vec3 f = glm::transpose(R) * (vec3(0.0f, -9.81f, 0.0f) - a);
    float push = std::cos(doorAngle) * f.x + std::sin(doorAngle) * f.z;
    if (doorHand != 0) // (the hand carries it at its own pace, whatever pushes it)
      doorSpin = doorHand * DOOR_HAND_SPEED;
    else if (doorAngle >= DOOR_OPEN_ANGLE - 0.01f && push > -DOOR_CATCH) // (held open by the catch)
      doorSpin = std::max(doorSpin, 0.0f);
    else
      doorSpin += 1.5f / DOOR_WIDTH * push * dtf;
    doorSpin *= std::exp(-DOOR_DAMPING * dtf);
    doorAngle += doorSpin * dtf;
    if (doorHand > 0 && doorAngle >= DOOR_OPEN_ANGLE) { // the hand lets go at the stop
      doorHand = 0;
      doorSpin = 0.0f;
    }
    if (doorHand < 0 && doorAngle <= 0.0f) { // and pushes it home
      doorHand = 0;
      doorSpin = 0.0f;
      doorAngle = 0.0f;
      doorLatched = true;
    }
    if (doorAngle > DOOR_OPEN_ANGLE) { // against the open stop
      doorAngle = DOOR_OPEN_ANGLE;
      doorSpin = doorSpin > 0.0f ? -doorSpin * DOOR_BOUNCE_OPEN : doorSpin;
    }
    if (doorAngle < 0.0f) { // shut: it latches if it comes gently, else it bounces back open a little
      doorAngle = 0.0f;
      if (doorSpin < 0.0f) {
        if (-doorSpin < DOOR_LATCH_SPEED) {
          doorLatched = true;
          doorSpin = 0.0f;
        } else {
          doorSpin = -doorSpin * DOOR_BOUNCE_SHUT;
        }
      }
    }
  }
  placeDoor();
}

void RV::placeDoor() {
  setPartTransform(doorPart, glm::rotate(glm::translate(mat4(1.0f), DOOR_HINGE), -doorAngle, vec3(0.0f, 1.0f, 0.0f)));
  hull->setEnabled(HULL_DOOR, doorAngle < 0.05f);
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
  mat4 frame(vec4(WHEEL_X, 0.0f), vec4(WHEEL_Y, 0.0f), vec4(WHEEL_Z, 0.0f), vec4(WHEEL_ORIGIN, 1.0f));
  setPartTransform(steeringWheelPart, glm::rotate(frame, steeringWheelAngle(), vec3(0.0f, 0.0f, 1.0f)));
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
  keyTurn += ((keyOn ? (starterNow() > 0.5f ? 1.3f : 1.0f) : 0.0f) - keyTurn) *
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
  placeCockpitCamera(*camera, false);
}

void RV::placeCockpitCamera(Camera &camera, bool copilot) const {
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
  camera.setCarrier(carrier *
                    mat3(glm::rotate(mat4(1.0f), 3.14159265f, vec3(0.0f, 1.0f, 0.0f))));
  vec3 eye = position + vec3(rotation * vec4(copilot ? -EYE_X : EYE_X, EYE_Y, EYE_Z, 0.0f));
  camera.reposition(eye.x, eye.y, eye.z);
}

void RV::control(vec2 dir, float up, float cameraYaw) {
  throttle = -dir.y; // W is forward
  steering = dir.x;  // D is right
}

void RV::update(double dt) {
  if (replica) {
    updateReplica(dt);
    return;
  }
  // Only the input is read here: the stage moves the RV through contactFloor()
  GameObject::update(dt);
  updateCockpit(dt);
  placeSteeringWheel();
  updateHeadlights(dt);
  updateHandbrake(dt);
  updateDoor(dt);
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
    if (flatTires[i]) // gone: the wheel is a prop of its own now (see punctureTire)
      continue;
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

// The tyre bursts and the wheel leaves: it is thrown out sideways and a little up, and rolls away
// on the floor like the pieces of an explosion (updateDebris); what is left is a bare corner
void RV::punctureTire(int wheel) {
  if (wheel < 0 || wheel > 3 || flatTires[wheel])
    return;
  placeWheels(); // (where the wheel is now: it starts from there)
  flatTires[wheel] = true;
  if (body)
    body->setLost(wheel, true);
  if (hasWheels && !exploded) {
    vec3 outward = vec3(rotation * vec4(wheel % 2 == 0 ? -1.0f : 1.0f, 0.0f, 0.0f, 0.0f)); // (-x: the right-hand side)
    size_t before = debris.size();
    launchPart(wheelParts[wheel], wheelHub(wheel) - outward, 2.5f, 4.5f);
    if (debris.size() > before) { // it rolls: it spins about its axle, the way it travels
      Debris &d = debris.back();
      vec3 along = d.velocity - outward * dot(d.velocity, outward);
      d.axis = outward * (dot(cross(vec3(0.0f, 1.0f, 0.0f), outward), along) >= 0.0f ? 1.0f : -1.0f);
      d.spin = std::max(length(vec3(along.x, 0.0f, along.z)), 3.0f) / WHEEL_RADIUS;
    }
  }
  updateTireLamp();
}

void RV::repairTire(int wheel) {
  if (!flatTires[wheel])
    return;
  flatTires[wheel] = false;
  if (body)
    body->setLost(wheel, false);
  for (size_t i = 0; i < debris.size(); i++) // (the wheel is back on its hub)
    if (debris[i].part == wheelParts[wheel])
      debris.erase(debris.begin() + i--);
}

void RV::repairTires() {
  for (int i = 0; i < 4; i++)
    repairTire(i);
  placeWheels();
  updateTireLamp();
}

void RV::setTireLampModel(std::shared_ptr<Model> lamp) {
  tireLampPart = addPart(lamp, 2); // emissive
  hasTireLamp = true;
  updateTireLamp();
}

// The orange lamp is lit while a wheel is missing (the dashboard goes with the explosion)
void RV::updateTireLamp() {
  if (!hasTireLamp)
    return;
  bool missing = false;
  for (bool lost : flatTires)
    missing = missing || lost;
  setPartVisible(tireLampPart, missing && !exploded);
}

// Each missing wheel's bare corner throws sparks while it scrapes along the floor: yellow specks
// thrown back from the hub, that fall behind the vehicle in a trace
void RV::updateSparks() {
  vec3 horizontal(velocity.x, 0.0f, velocity.z);
  float speed = length(horizontal);
  vec3 away = speed > 1e-3f ? -horizontal / speed : vec3(0.0f);
  const VehicleBody::Params &params = body->getParams();
  for (int i = 0; i < 4; i++) {
    ParticleEmitter &emitter = *sparks[i];
    sparkLit[i] = false;
    if (!flatTires[i] || exploded || !body->getWheels()[i].onGround || speed < SPARK_MIN_SPEED) {
      emitter.setRate(0.0f);
      continue;
    }
    const VehicleBody::WheelState &wheel = body->getWheels()[i];
    const vec3 &anchor = params.wheels[i].anchor;
    vec3 contact = position + vec3(rotation * vec4(anchor.x, anchor.y - wheel.length - (WHEEL_RADIUS - params.lostDrop),
                                                   anchor.z, 0.0f));
    sparkSpot[i] = contact + vec3(0.0f, 0.08f, 0.0f);
    sparkLit[i] = true;
    emitter.setPosition(sparkSpot[i]);
    emitter.setDirection(away * SPARK_BACKWARDS + vec3(0.0f, SPARK_UPWARDS, 0.0f));
    emitter.setBaseVelocity(velocity * SPARK_INHERIT);
    emitter.setRate(SPARK_RATE * std::min(speed, DUST_MAX_RATE_SPEED) / 10.0f);
  }
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

void RV::ensureBody() {
  if (body)
    return;
  body.reset(new VehicleBody(vehicleParams(gravity, maxSpeed)));
  body->place(position, facing);
  for (int i = 0; i < 4; i++)
    body->setLost(i, flatTires[i]);
}

bool RV::contactFloor(const Stage &stage, double dt) {
  ensureBody();
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
  updateSparks();
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
                         states[i].onGround ? "*" : "", isTireFlat((int)i) ? " SIN RUEDA" : "");
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
  properties.push_back(Property::info("Ruedas perdidas", [this]() {
    const char *names[] = {"DD", "DI", "TD", "TI"};
    std::string flat;
    for (int i = 0; i < 4; i++)
      if (flatTires[i])
        flat += std::string(flat.empty() ? "" : " ") + names[i];
    return flat.empty() ? std::string("ninguna") : flat;
  }));
  properties.push_back(Property::action("Perder una rueda al azar", [this]() {
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

// ----------------------------------------------------------------- network
void RV::writeNetState(NetWriter &out) const {
  const VehicleBody::Params &p = body ? body->getParams() : vehicleParams(0, 0);
  for (int i = 0; i < 4; i++) {
    out.f32(body ? body->getWheels()[i].length : p.restLength);
    out.f32(body ? body->getWheels()[i].steer : 0.0f);
    out.boolean(body ? body->getWheels()[i].onGround : true);
  }
  out.vec3(body ? body->getAngularVelocity() : vec3(0.0f));
  uint32_t flags = (handbrakeOn ? 1 : 0) | (occupied ? 2 : 0) | (keyOn ? 4 : 0) | (engineOn ? 8 : 0) |
                   (headlightsOn ? 16 : 0) | (parkedLights ? 32 : 0) | (wrecked ? 64 : 0) |
                   (exploded ? 128 : 0) | (onFire ? 256 : 0) | (damagedWindshield ? 512 : 0) |
                   (doorLatched ? 1024 : 0) | (cameraView == CameraView::Chase ? 2048 : 0) | (copilotOccupied ? 8192 : 0) |
                   (engineSim.getPhase() == EngineSimulator::Phase::Cranking ? 4096 : 0);
  for (int i = 0; i < 4; i++)
    if (flatTires[i])
      flags |= 1u << (16 + i);
  out.u32(flags);
  out.f32(lampLevel);
  out.f32(fuel);
  out.f32(engineSim.getRpm());
  out.f32(engineSim.getLoad());
  out.f32(engineSim.getStarter());
  out.f32(engineSim.getFire());
  out.f32(doorAngle);
}

void RV::readNetState(NetReader &in) {
  for (int i = 0; i < 4; i++) {
    netWheelLength[i] = in.f32();
    netWheelSteer[i] = in.f32();
    netWheelGround[i] = in.boolean();
  }
  netAngular = in.vec3();
  uint32_t flags = in.u32();
  float lamps = in.f32(), tank = in.f32();
  netRpm = in.f32();
  netLoad = in.f32();
  netStarter = in.f32();
  netFire = in.f32();
  doorWanted = in.f32();
  if (!in.isOk())
    return;
  netCranking = flags & 4096;
  // What happened to it: the crash and the explosion are done here too (the pieces that fly are
  // each client's own)
  if ((flags & 64) && !wrecked)
    wreck();
  if ((flags & 128) && !exploded)
    explodeEngine();
  bool broken = flags & 512;
  if (broken != damagedWindshield) {
    damagedWindshield = broken;
    updateWindshieldParts();
  }
  handbrakeOn = flags & 1;
  copilotOccupied = flags & 8192;
  occupied = flags & 2;
  keyOn = flags & 4;
  engineOn = flags & 8;
  headlightsOn = flags & 16;
  parkedLights = flags & 32;
  onFire = flags & 256;
  lampLevel = lamps;
  fuel = tank;
  for (int i = 0; i < 4; i++) { // (a wheel that went: it flies off here too)
    bool lost = flags & (1u << (16 + i));
    if (lost && !flatTires[i])
      punctureTire(i);
    else if (!lost && flatTires[i])
      repairTire(i);
  }
  updateTireLamp();
  if (doorLatched != bool(flags & 1024)) {
    doorLatched = flags & 1024;
    if (doorLatched)
      doorAngle = 0.0f;
  }
  CameraView view = (flags & 2048) ? CameraView::Chase : CameraView::Cockpit;
  if (view != cameraView)
    setCameraView(view);
  updateLights();
}

// The copy on a client: its chassis, wheels, door and lamps are as the server says, and here the
// dust, the engine's sound, the gauges and the pieces that fly off in an explosion are made
void RV::updateReplica(double dt) {
  GameObject::update(dt);
  ensureBody();
  body->setState(position, mat3(rotation), velocity, netAngular);
  for (int i = 0; i < 4; i++)
    body->setWheel(i, netWheelLength[i], netWheelSteer[i], netWheelGround[i]);
  float follow = std::min(1.0f, 15.0f * (float)dt);
  doorAngle += (doorWanted - doorAngle) * follow;
  if (hasDoor && !exploded)
    placeDoor();
  if (engineSound)
    engineSound->update(position + vec3(0.0f, 1.0f, 0.0f), netRpm, netLoad, netStarter, netFire, netCranking);
  updateCockpit(dt);
  placeSteeringWheel();
  updateHandbrake(dt);
  updateFire(dt);
  placeWheels();
  if (stage) {
    updateDust(*stage);
    updateSparks();
    updateDebris(*stage, dt);
  }
}
