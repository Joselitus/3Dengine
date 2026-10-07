#include "Mosquito.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "RV.h"
#include "TextFormat.h"

using namespace glm;
using namespace std;

constexpr float Mosquito::DEATH_SPEED_CHANGE;
constexpr float Mosquito::DIVE_SPEED;

namespace {
const float PI = 3.14159265f;
const float GRAVITY = 25.0f;       // when it is dead and falls
const float TURN_RATE = 3.5f;      // how fast it turns to face where it wants (rad/s)
const float HOVER_PITCH = -0.15f;  // hovering it holds its head up and its abdomen down (radians)
// Its collision box: the thorax and the abdomen (the legs and the wings go through things)
const vec3 BOX_HALF(0.42f, 0.42f, 1.05f);
const vec3 BOX_CENTER(0.0f, -0.1f, -0.45f);
const float WORLD_LIMIT = 72.0f;   // it roams within +-this (the day map's terrain is +-90 m)
const float LAY_PITCH = -0.45f;    // laying, its abdomen hangs down towards the water

// The angle from `a` to `b` the short way round (radians)
float angleTo(float a, float b) {
  float d = std::fmod(b - a + PI, 2.0f * PI);
  if (d < 0.0f)
    d += 2.0f * PI;
  return d - PI;
}

float approach(float value, float target, float maxStep) {
  return value + glm::clamp(target - value, -maxStep, maxStep);
}

// The bind pose of the legs: L0, R0, L1, R1, L2, R2 (the right ones mirror the left ones)
InsectLegs::Pose bindLegs() {
  InsectLegs::Pose pose;
  for (int pair = 0; pair < 3; pair++) {
    for (float side : {1.0f, -1.0f}) {
      std::vector<vec3> leg;
      for (int j = 0; j < 5; j++)
        leg.push_back(vec3(side * Mosquito::LEG_JOINTS[pair][j][0], Mosquito::LEG_JOINTS[pair][j][1],
                           Mosquito::LEG_JOINTS[pair][j][2]));
      pose.push_back(leg);
    }
  }
  return pose;
}
} // namespace

// generate_mosquito.py, LEGS
const float Mosquito::LEG_JOINTS[3][5][3] = {
    {{0.12f, -0.22f, 0.20f}, {0.40f, -0.10f, 0.52f}, {0.72f, -0.55f, 0.95f}, {0.82f, -1.20f, 1.22f}, {0.88f, -1.68f, 1.35f}},
    {{0.15f, -0.26f, 0.00f}, {0.62f, 0.02f, 0.05f}, {1.12f, -0.52f, 0.00f}, {1.32f, -1.22f, -0.10f}, {1.42f, -1.70f, -0.14f}},
    {{0.12f, -0.22f, -0.20f}, {0.48f, 0.02f, -0.52f}, {0.80f, -0.30f, -1.08f}, {0.90f, 0.08f, -1.58f}, {0.84f, 0.52f, -1.90f}},
};

Mosquito::Mosquito(shared_ptr<Model> body, shared_ptr<Model> abdomen, shared_ptr<Model> left,
                   shared_ptr<Model> right, const vector<shared_ptr<Model>> &legSegments,
                   SoundEngine &engine)
    : DynamicGameObject(body, make_shared<Box>(BOX_HALF, BOX_CENTER)), legs(bindLegs()),
      random(std::random_device()()), soundEngine(engine), buzz(new BuzzSynth()) {
  // (the middle of each part, in the body's frame: what its piece spins about if it blows up)
  partCentres.push_back(vec3(0.0f, -0.1f, 0.5f)); // head and thorax
  abdomenPart = addPart(abdomen);
  partCentres.push_back(vec3(0.0f, -0.2f, -1.0f));
  wingLeft = addPart(left);
  partCentres.push_back(vec3(WING_HINGE_X + 0.8f, WING_HINGE_Y, -0.05f));
  wingRight = addPart(right);
  partCentres.push_back(vec3(-WING_HINGE_X - 0.8f, WING_HINGE_Y, -0.05f));
  const InsectLegs::Pose &bind = legs.getBind();
  for (size_t i = 0; i < legSegments.size(); i++) {
    legParts.push_back(addPart(legSegments[i]));
    const auto &leg = bind[std::min(i / 4, bind.size() - 1)];
    size_t j = i % 4;
    partCentres.push_back((leg[j] + leg[j + 1]) * 0.5f);
  }
  makeEmitters();
  legs.setFloor([this](float x, float z, float &height) {
    return floorHeight && floorHeight(x, z, height);
  });
  setGravity(0.0f); // it flies
  setMass(40.0f);
  setMaxSpeed(DIVE_SPEED + 1.0f);
  setMaxAcceleration(30.0f);
  circleDirection = uniform(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
  updateWings(0.0);
}

// The explosion's fire, smoke and splash of fuel and blood (all sent out at once: burst)
void Mosquito::makeEmitters() {
  ParticleSettings fire;
  fire.lifeMin = 0.35f;
  fire.lifeMax = 0.85f;
  fire.speedMin = 3.0f;
  fire.speedMax = 9.0f;
  fire.spread = PI; // every way
  fire.sizeStart = 0.4f;
  fire.sizeEnd = 1.8f;
  fire.color = vec3(1.0f, 0.55f, 0.12f);
  fire.alpha = 0.95f;
  fire.gravity = -3.0f; // hot: it rises
  fire.drag = 3.0f;
  fire.maxParticles = 160;
  ParticleSettings smoke;
  smoke.lifeMin = 1.5f;
  smoke.lifeMax = 3.0f;
  smoke.speedMin = 1.0f;
  smoke.speedMax = 4.0f;
  smoke.spread = PI;
  smoke.sizeStart = 0.5f;
  smoke.sizeEnd = 2.8f;
  smoke.color = vec3(0.18f, 0.16f, 0.15f);
  smoke.alpha = 0.75f;
  smoke.gravity = -1.2f;
  smoke.drag = 1.2f;
  smoke.maxParticles = 80;
  ParticleSettings splash; // fuel and blood: dark red drops that fall
  splash.lifeMin = 0.8f;
  splash.lifeMax = 1.5f;
  splash.speedMin = 4.0f;
  splash.speedMax = 10.0f;
  splash.spread = PI;
  splash.sizeStart = 0.07f;
  splash.sizeEnd = 0.05f;
  splash.color = vec3(0.30f, 0.05f, 0.03f);
  splash.alpha = 1.0f;
  splash.fadeStart = 0.85f;
  splash.gravity = 14.0f;
  splash.drag = 0.3f;
  splash.maxParticles = 220;
  unsigned seed = (unsigned)random();
  emitters.push_back(make_shared<ParticleEmitter>(fire, seed));
  emitters.push_back(make_shared<ParticleEmitter>(smoke, seed + 1));
  emitters.push_back(make_shared<ParticleEmitter>(splash, seed + 2));
}

// The bang of the explosion: made once (noise through a low-pass that closes, a quick attack and a
// long decay, with a low thump and some crackle), shared by every mosquito
static shared_ptr<AudioClip> bangClip() {
  static shared_ptr<AudioClip> clip;
  if (clip)
    return clip;
  clip = make_shared<AudioClip>();
  clip->channels = 1;
  clip->sampleRate = 44100;
  const int n = 44100 * 2;
  clip->samples.resize(n);
  std::mt19937 noise(99u);
  std::uniform_real_distribution<float> white(-1.0f, 1.0f);
  float low = 0.0f, low2 = 0.0f, peak = 0.0f;
  for (int i = 0; i < n; i++) {
    float t = i / 44100.0f;
    float cutoff = 0.02f + 0.35f * std::exp(-t / 0.08f); // bright crack, then a dull roar
    low += (white(noise) - low) * cutoff;
    low2 += (low - low2) * cutoff;
    float envelope = std::min(1.0f, t / 0.003f) * std::exp(-t / 0.45f);
    float thump = 0.9f * std::sin(2.0f * PI * (48.0f - 18.0f * t) * t) * std::exp(-t / 0.35f);
    float crackle = (white(noise) > 0.995f && t < 0.6f) ? white(noise) * 0.6f * std::exp(-t / 0.3f) : 0.0f;
    float v = std::tanh(2.5f * (low2 * 3.0f * envelope + thump * std::min(1.0f, t / 0.01f) + crackle));
    clip->samples[i] = v;
    peak = std::max(peak, std::fabs(v));
  }
  for (float &v : clip->samples)
    v *= 0.9f / std::max(peak, 1e-3f);
  return clip;
}

void Mosquito::setGrowth(float g) {
  growth = clamp(g, 0.0f, 1.0f);
  float size = mix(BABY_SCALE, 1.0f, growth);
  setScale(size);
  setMass(std::max(40.0f * size * size * size, 0.05f));
}

float Mosquito::groundAt(float x, float z) const {
  float h;
  if (floorHeight && floorHeight(x, z, h))
    return h;
  return position.y - MIN_CLEARANCE; // no floor: as if it were just under it
}

void Mosquito::enterBehavior(Behavior next) {
  if (next == behavior)
    return;
  if (next != Behavior::Feed && next != Behavior::Bite)
    releaseBiteSlot(); // (it is no longer going to bite: its place is free)
  behavior = next;
  stateTime = 0.0f;
  sucking = false;
  if (next == Behavior::Stalk) {
    // it joins the circle where it is, and waits a while before diving
    vec3 target = targetPosition ? targetPosition() : position;
    circleAngle = std::atan2(position.z - target.z, position.x - target.x);
    diveWait = uniform(DIVE_WAIT_MIN, DIVE_WAIT_MAX);
  } else if (next == Behavior::Wander) {
    hasWaypoint = false;
  } else if (next == Behavior::Lay) {
    // its eggs drop one by one while it hovers over the water
    eggsToLay = std::uniform_int_distribution<int>(EGGS_MIN, EGGS_MAX)(random);
    nextEgg = LAY_TIME / (eggsToLay + 1);
  }
}

// Takes the free place round the head nearest to the side it comes from (false: all taken)
bool Mosquito::claimBiteSlot(const vec3 &head) {
  if (!biteSlots)
    return true;
  vec3 out = position - head;
  out.y = 0.0f;
  out = length(out) > 0.01f ? normalize(out) : vec3(0.0f, 0.0f, 1.0f);
  int best = -1;
  float bestDot = -2.0f;
  for (int k = 0; k < BiteSlots::SLOTS; k++) {
    if (biteSlots->owner[k] && biteSlots->owner[k] != this)
      continue;
    float a = 2.0f * PI * k / BiteSlots::SLOTS;
    float d = dot(out, vec3(std::sin(a), 0.0f, std::cos(a)));
    if (d > bestDot) {
      bestDot = d;
      best = k;
    }
  }
  if (best < 0)
    return false;
  releaseBiteSlot();
  biteSlot = best;
  biteSlots->owner[best] = this;
  float a = 2.0f * PI * best / BiteSlots::SLOTS;
  biteDir = vec3(std::sin(a), 0.0f, std::cos(a));
  biteHeight = best % 2 == 0 ? -0.14f : 0.04f; // cheek, temple
  return true;
}

void Mosquito::releaseBiteSlot() {
  if (biteSlots && biteSlot >= 0 && biteSlots->owner[biteSlot] == this)
    biteSlots->owner[biteSlot] = nullptr;
  biteSlot = -1;
}

bool Mosquito::isAttackTime() const {
  if (!hourQuery)
    return true;
  float h = hourQuery();
  return (h >= DAWN_FROM && h < DAWN_TO) || (h >= DUSK_FROM && h < DUSK_TO);
}

// It can go for the fuel: the vehicle has some, the player is neither in it nor near it, and it
// is not too far away
bool Mosquito::wantsFuel() const {
  if (!vehicle || vehicle->getFuel() <= 0.0f || !targetPosition || !isGrown())
    return false;
  // (full, it leaves; it comes back once it is hungry again)
  if (stomach >= (behavior == Behavior::Siphon ? 1.0f : STOMACH_HUNGRY))
    return false;
  if (playerInVehicle && playerInVehicle())
    return false;
  vec3 car = vehicle->getPosition(), player = targetPosition();
  return length(vec2(player.x - car.x, player.z - car.z)) > FUEL_SAFE_DISTANCE &&
         length(car - position) < VEHICLE_SENSE;
}

// Where its body goes to suck: facing the wall the cap is on, level, with the tip of its
// proboscis right in the cap
void Mosquito::siphonSpot(vec3 &spot, float &faceYaw) const {
  vec3 cap, normal;
  vehicle->fuelCap(cap, normal);
  faceYaw = std::atan2(-normal.x, -normal.z);
  vec3 tip = vec3(0.0f, PROBOSCIS_TIP_Y, PROBOSCIS_TIP_Z) * scale;
  spot = cap - vec3(rotate(mat4(1.0f), faceYaw, vec3(0.0f, 1.0f, 0.0f)) * vec4(tip, 0.0f));
}

// The nearest water it can lay in within WATER_SENSE, or -1
int Mosquito::senseWater() const {
  int best = -1;
  float bestDistance = WATER_SENSE;
  for (size_t i = 0; i < waterSpots.size(); i++) {
    float d = length(vec2(waterSpots[i].x - position.x, waterSpots[i].z - position.z));
    if (waterCooldown[i] <= 0.0f && d < bestDistance) {
      best = (int)i;
      bestDistance = d;
    }
  }
  return best;
}

// Where it dives: the head of the player on foot, or a point above the vehicle
vec3 Mosquito::aimPoint() const {
  vec3 target = targetPosition ? targetPosition() : position;
  bool inVehicle = playerInVehicle && playerInVehicle();
  return target + vec3(0.0f, inVehicle ? VEHICLE_AIM_HEIGHT : HEAD_HEIGHT, 0.0f);
}

vec3 Mosquito::wanderVelocity() {
  vec3 to = waypoint - position;
  if (!hasWaypoint || length(to) < 2.0f * scale) {
    // a new point round home, uniform over the disc
    float roam = isGrown() ? WATER_ROAM_RADIUS : JUVENILE_ROAM;
    float a = uniform(0.0f, 2.0f * PI), r = roam * std::sqrt(uniform(0.0f, 1.0f));
    waypoint = home + vec3(std::cos(a) * r, 0.0f, std::sin(a) * r);
    // (not out over the edge of the desert, where nothing is drawn)
    waypoint.x = clamp(waypoint.x, -WORLD_LIMIT, WORLD_LIMIT);
    waypoint.z = clamp(waypoint.z, -WORLD_LIMIT, WORLD_LIMIT);
    waypoint.y = groundAt(waypoint.x, waypoint.z) +
                 uniform(WANDER_HEIGHT_MIN, WANDER_HEIGHT_MAX) * scale;
    hasWaypoint = true;
    to = waypoint - position;
  }
  float d = length(to);
  // (a young one flies slower, and arrives sooner)
  return to / std::max(d, 0.001f) * FLY_SPEED * 0.7f * (0.3f + 0.7f * scale) *
         std::min(1.0f, d / (3.0f * scale));
}

// Round the target, on the circle, and a little up and down
vec3 Mosquito::stalkVelocity(const vec3 &target, double dt) {
  circleAngle += circleDirection * STALK_TURN_RATE * (float)dt;
  vec3 spot = target + vec3(std::cos(circleAngle), 0.0f, std::sin(circleAngle)) * STALK_RADIUS;
  spot.y = groundAt(spot.x, spot.z) + STALK_HEIGHT + 0.6f * std::sin((float)time * 1.3f);
  vec3 want = (spot - position) * 1.6f;
  float speed = length(want), top = FLY_SPEED * 1.6f; // it keeps up with a vehicle
  if (speed > top)
    want *= top / speed;
  return want;
}

void Mosquito::update(double dt) {
  if (exploded) {
    GameObject::update(dt);
    updateDebris(dt);
    return;
  }
  if (behavior == Behavior::Dead) {
    updateDead(dt);
    return;
  }
  float dtf = (float)dt;
  stateTime += dtf;
  bool dead = playerDead && playerDead();
  bool inVehicle = playerInVehicle && playerInVehicle();
  vec3 target = targetPosition ? targetPosition() : position;
  float distance = length(vec2(target.x - position.x, target.z - position.z));
  bool attack = isAttackTime();
  for (float &cooldown : waterCooldown)
    cooldown -= dtf;

  // what to do
  bool grown = isGrown(); // (a young one only flutters about)
  bool hunts = grown && targetPosition && !dead && attack && distance < DETECT_RANGE;
  bool fuel = wantsFuel();
  if (!sucking) // it digests what it sucked
    stomach = std::max(0.0f, stomach - dtf / DIGEST_TIME);
  // a young one wants blood: it bites the player on foot, at any hour
  feedCooldown = std::max(0.0f, feedCooldown - dtf);
  bool feeds = !grown && targetPosition && !dead && !inVehicle && distance < YOUNG_DETECT &&
               feedCooldown <= 0.0f;
  if (!grown && behavior != Behavior::Bite)
    blood = std::max(0.0f, blood - dtf / YOUNG_DIGEST_TIME);
  if (behavior == Behavior::Bite) {
    updateBite(dt);
    return;
  }
  // the tyres: now and then, while the player drives near it
  if (grown && vehicle && inVehicle && length(vehicle->getPosition() - position) < TIRE_SENSE) {
    if ((tireCheck -= dtf) <= 0.0f) {
      tireCheck = TIRE_CHECK_INTERVAL;
      if (behavior != Behavior::Dive && behavior != Behavior::TireAttack &&
          uniform(0.0f, 1.0f) < TIRE_ATTACK_CHANCE) {
        tireTarget = std::uniform_int_distribution<int>(0, 3)(random);
        enterBehavior(Behavior::TireAttack);
      }
    }
  } else {
    tireCheck = TIRE_CHECK_INTERVAL;
  }
  switch (behavior) {
  case Behavior::Wander:
    if (feeds && claimBiteSlot(target + vec3(0.0f, HEAD_HEIGHT, 0.0f))) {
      enterBehavior(Behavior::Feed);
    } else if (feeds) {
      feedCooldown = 1.0f; // (no room round his head: it tries again in a moment)
    } else if (hunts) {
      enterBehavior(Behavior::Stalk);
    } else if (fuel) {
      enterBehavior(Behavior::Siphon);
    } else if (grown && blood >= BLOOD_TO_LAY && (water = senseWater()) >= 0) {
      enterBehavior(Behavior::ToWater);
    }
    break;
  case Behavior::ToWater: {
    vec3 spot = waterSpots[water];
    if (hunts)
      enterBehavior(Behavior::Stalk);
    else if (fuel)
      enterBehavior(Behavior::Siphon);
    else if (length(vec2(spot.x - position.x, spot.z - position.z)) < 1.0f)
      enterBehavior(Behavior::Lay);
    break;
  }
  case Behavior::Feed: {
    vec3 head = target + vec3(0.0f, HEAD_HEIGHT, 0.0f);
    if (biteSlot < 0) { // (without shared places: it lands on the side it comes from)
      vec3 out = position - head;
      out.y = 0.0f;
      if (length(out) > 0.01f)
        biteDir = normalize(out);
    }
    if (inVehicle || dead || distance > YOUNG_DETECT * 1.5f) {
      enterBehavior(Behavior::Wander);
    } else if (length(biteSpot(head, 0.0f) - position) < 0.15f + 0.5f * scale) {
      enterBehavior(Behavior::Bite);
      setCollidable(false); // (it clings to him: it does not push him)
      updateBite(dt);
      return;
    }
    break;
  }
  case Behavior::Siphon:
    if (hunts)
      enterBehavior(Behavior::Stalk);
    else if (!fuel) // the player came back (or got in), the tank is dry or it is full: away
      enterBehavior(Behavior::Retreat);
    break;
  case Behavior::TireAttack:
    if (!inVehicle || !vehicle) {
      enterBehavior(Behavior::Retreat);
    } else if (length(vehicle->wheelHub(tireTarget) - position) < TIRE_REACH) {
      vehicle->punctureTire(tireTarget); // it bursts it... and blows up with it
      explode();
      return;
    } else if (stateTime > TIRE_TIMEOUT) {
      enterBehavior(Behavior::Retreat);
    }
    break;
  case Behavior::Lay:
    if (hunts) {
      enterBehavior(Behavior::Stalk);
    } else if (fuel) {
      enterBehavior(Behavior::Siphon);
    } else if (stateTime > LAY_TIME) {
      waterCooldown[water] = LAY_COOLDOWN;
      blood = 0.0f; // (the eggs took it)
      enterBehavior(Behavior::Wander);
    }
    break;
  case Behavior::Stalk:
    if (dead || distance > LOSE_RANGE || !attack)
      enterBehavior(Behavior::Wander);
    else if ((diveWait -= dtf) <= 0.0f)
      enterBehavior(Behavior::Dive);
    break;
  case Behavior::Dive: {
    float reach = length(aimPoint() - position);
    if (dead) {
      enterBehavior(Behavior::Retreat);
    } else if (!inVehicle && reach < BITE_RANGE && stomach >= KAMIKAZE_FUEL) {
      explode(); // full of fuel: it blows up next to him (the blast kills him: see explode)
      return;
    } else if (!inVehicle && reach < BITE_RANGE) {
      if (playerCaught)
        playerCaught(); // it bites
      blood = 1.0f;   // ...and drinks
      enterBehavior(Behavior::Retreat);
    } else if ((inVehicle && reach < VEHICLE_PULL_UP) || stateTime > DIVE_TIMEOUT) {
      enterBehavior(Behavior::Retreat);
    }
    break;
  }
  case Behavior::Retreat:
    if (stateTime > RETREAT_TIME)
      enterBehavior(dead || !attack || !grown ? Behavior::Wander : Behavior::Stalk);
    break;
  case Behavior::Bite: // (see updateBite)
  case Behavior::Dead:
    break;
  }

  // how it moves
  vec3 want(0.0f);
  float responsiveness = 2.5f;
  switch (behavior) {
  case Behavior::Wander:
    want = wanderVelocity();
    break;
  case Behavior::ToWater: {
    vec3 spot = waterSpots[water];
    vec3 to = vec3(spot.x, groundAt(spot.x, spot.z) + WANDER_HEIGHT_MIN, spot.z) - position;
    float d = length(to);
    want = to / std::max(d, 0.001f) * FLY_SPEED * 0.8f * std::min(1.0f, d / 3.0f);
    break;
  }
  case Behavior::Lay: {
    // low over the middle of the water, dipping slowly, dropping its eggs one by one on it
    vec3 spot = waterSpots[water];
    if (eggsToLay > 0 && stateTime >= nextEgg) {
      float a = uniform(0.0f, 2.0f * PI), r = 1.6f * std::sqrt(uniform(0.0f, 1.0f));
      if (eggLayer && eggLayer(spot + vec3(std::cos(a) * r, 0.0f, std::sin(a) * r))) {
        eggs++;
        eggsToLay--;
        nextEgg += LAY_TIME / (eggsToLay + 1);
      } else {
        eggsToLay = 0; // no room for more
      }
    }
    vec3 hover(spot.x, spot.y + LAY_HEIGHT + 0.15f * std::sin((float)time * 1.7f), spot.z);
    want = (hover - position) * 2.0f;
    break;
  }
  case Behavior::Siphon: {
    // to its spot by the cap, slowly at the end; there it sucks
    vec3 spot;
    float faceYaw;
    siphonSpot(spot, faceYaw);
    vec3 to = spot - position;
    float d = length(to);
    want = to * 2.0f;
    if (d * 2.0f > FLY_SPEED)
      want *= FLY_SPEED / (d * 2.0f);
    responsiveness = 4.0f;
    sucking = d < SIPHON_REACH;
    if (sucking) {
      float before = vehicle->getFuel();
      vehicle->setFuel(before - SIPHON_RATE * dtf);
      float taken = before - vehicle->getFuel();
      fuelTaken += taken;
      stomach = std::min(1.0f, stomach + taken / STOMACH_CAPACITY); // its abdomen swells
    }
    break;
  }
  case Behavior::TireAttack: {
    // beside the wheel, a little out from the side and above it (it can't fly lower), aiming
    // where it is going to be
    vec3 hub = vehicle->wheelHub(tireTarget);
    vec3 side = vehicle->getPose().rotation * vec3(tireTarget % 2 == 0 ? -1.0f : 1.0f, 0.0f, 0.0f);
    // (far enough out that its body does not hit the vehicle's: its proboscis reaches the wheel)
    vec3 spot = hub + side * 1.9f + vec3(0.0f, 1.3f, 0.0f) + vehicle->getVelocity() * 0.3f;
    // on the other side of the vehicle it would get stuck against it: it goes over the roof first
    float across = dot(position - vehicle->getPosition(), side);
    if (across < 1.0f)
      spot.y = vehicle->getPosition().y + VEHICLE_ROOF + 1.5f;
    vec3 to = spot - position;
    want = to / std::max(length(to), 0.001f) * DIVE_SPEED;
    responsiveness = 4.0f;
    break;
  }
  case Behavior::Stalk:
    want = stalkVelocity(target, dt);
    break;
  case Behavior::Feed: {
    // quick, to beside his head
    vec3 to = biteSpot(target + vec3(0.0f, HEAD_HEIGHT, 0.0f), 0.0f) - position;
    want = to * 3.0f;
    float top = 4.5f;
    if (length(want) > top)
      want *= top / length(want);
    responsiveness = 6.0f;
    break;
  }
  case Behavior::Dive: {
    // full speed, braking as it gets near (it bites from BITE_RANGE: it need not ram him)
    vec3 to = aimPoint() - position;
    float reach = length(to);
    float speed = std::min(DIVE_SPEED, std::max(3.0f, (reach - 0.5f * BITE_RANGE) * 3.0f));
    want = to / std::max(reach, 0.001f) * speed;
    responsiveness = 4.0f;
    break;
  }
  case Behavior::Retreat: {
    vec3 away = position - target;
    away.y = 0.0f;
    away = length(away) > 0.01f ? normalize(away) : vec3(1.0f, 0.0f, 0.0f);
    want = away * FLY_SPEED * 1.3f * std::max(scale, 0.5f) + vec3(0.0f, 4.0f * std::max(scale, 0.4f), 0.0f);
    break;
  }
  case Behavior::Bite: // (see updateBite)
  case Behavior::Dead:
    break;
  }
  steerTowards(want, responsiveness);
  velocity.y = clamp(velocity.y, -DIVE_SPEED, DIVE_SPEED);
  DynamicGameObject::update(dt);

  // never too low: its legs hang under it
  float floorY = groundAt(position.x, position.z) + MIN_CLEARANCE * scale;
  if (position.y < floorY) {
    position.y = floorY;
    if (velocity.y < 0.0f)
      velocity.y = 0.0f;
  }
  updateAttitude(dt);
  updateWings(dt);
  updateLegs(dt);
  updateAbdomen(dt);
  updateBuzz();
}

// Where its body goes to bite the player: on the side `biteDir` of his head, facing it, the tip of its
// proboscis on him (a little out from the head's middle), `pump` (0..1) further in
vec3 Mosquito::biteSpot(const vec3 &head, float pump) const {
  vec3 skin = head + biteDir * 0.3f + vec3(0.0f, biteHeight, 0.0f);
  float faceYaw = std::atan2(-biteDir.x, -biteDir.z);
  mat4 r = glm::rotate(mat4(1.0f), faceYaw, vec3(0.0f, 1.0f, 0.0f));
  r = glm::rotate(r, BITE_PITCH, vec3(1.0f, 0.0f, 0.0f));
  vec3 tip = vec3(r * vec4(0.0f, PROBOSCIS_TIP_Y, PROBOSCIS_TIP_Z, 0.0f)) * scale;
  vec3 forward = vec3(r * vec4(0.0f, 0.0f, 1.0f, 0.0f));
  return skin - tip + forward * (pump * 0.35f * scale);
}

// A young one biting: it clings beside the player's head (its legs holding on), faces him with
// its head down a little, and pumps its proboscis in and out while its abdomen fills with blood;
// it lets go after BITE_TIME, grown a little
void Mosquito::updateBite(double dt) {
  float dtf = (float)dt;
  bool dead = playerDead && playerDead();
  bool inVehicle = playerInVehicle && playerInVehicle();
  if (stateTime > BITE_TIME || dead || inVehicle || !targetPosition) {
    feedCooldown = FEED_COOLDOWN;
    setCollidable(true);
    enterBehavior(Behavior::Retreat);
    return;
  }
  blood = std::min(1.0f, blood + dtf / BITE_TIME);
  if (!isGrown())
    setGrowth(growth + dtf / BLOOD_GROWTH_TIME);
  vec3 head = targetPosition() + vec3(0.0f, HEAD_HEIGHT, 0.0f);
  // in and out, about twice a second, a quick thrust and a slow pull
  float phase = std::fmod(stateTime * 2.2f, 1.0f);
  float pump = phase < 0.25f ? phase / 0.25f : 1.0f - (phase - 0.25f) / 0.75f;
  position = biteSpot(head, pump);
  velocity = acceleration = vec3(0.0f);
  yaw = std::atan2(-biteDir.x, -biteDir.z);
  pitch = BITE_PITCH;
  roll = 0.0f;
  mat4 r = glm::rotate(mat4(1.0f), yaw, vec3(0.0f, 1.0f, 0.0f));
  rotation = glm::rotate(r, pitch, vec3(1.0f, 0.0f, 0.0f));
  GameObject::update(dt);
  updateWings(dt);
  updateLegs(dt);
  updateAbdomen(dt);
  updateBuzz();
}

// The abdomen swells with what is in the stomach (fuel and blood): thicker and a little longer,
// about where it joins the thorax
void Mosquito::updateAbdomen(double dt) {
  swell = approach(swell, clamp(stomach + 0.6f * blood, 0.0f, 1.0f), 0.5f * (float)dt);
  vec3 pivot(0.0f, ABDOMEN_PIVOT_Y, ABDOMEN_PIVOT_Z);
  mat4 m = glm::translate(mat4(1.0f), pivot);
  m = glm::scale(m, vec3(1.0f + ABDOMEN_SWELL_XY * swell, 1.0f + ABDOMEN_SWELL_XY * swell,
                         1.0f + ABDOMEN_SWELL_Z * swell));
  setPartTransform(abdomenPart, glm::translate(m, -pivot));
}

// It blows up: every part flies off on its own, the abdomen bursts, fire, smoke, a flash, a bang
void Mosquito::explode() {
  if (exploded)
    return;
  exploded = true;
  releaseBiteSlot();
  behavior = Behavior::Dead;
  buzzSound.reset();
  setCollidable(false);
  setGravity(0.0f);
  vec3 flying = velocity; // (the pieces carry on with it)
  velocity = acceleration = vec3(0.0f);
  frozen = glm::translate(mat4(1.0f), position) * rotation * glm::scale(mat4(1.0f), vec3(scale));
  vec3 centre = vec3(frozen * vec4(0.0f, -0.2f, -0.3f, 1.0f));
  float power = 1.0f + stomach; // (full of fuel it blows up harder)
  for (size_t i = 0; i < parts.size() && i < partCentres.size(); i++) {
    if (i == abdomenPart) { // it bursts: only the splash is left of it
      setPartVisible(i, false);
      continue;
    }
    Debris d;
    d.part = i;
    d.world = frozen * parts[i].local;
    d.centre = vec3(inverse(parts[i].local) * vec4(partCentres[i], 1.0f));
    vec3 at = vec3(d.world * vec4(d.centre, 1.0f));
    vec3 out = at - centre;
    out = length(out) > 1e-3f ? normalize(out) : vec3(0.0f, 1.0f, 0.0f);
    float speed = uniform(DEBRIS_SPEED_MIN, DEBRIS_SPEED_MAX) * (i == 0 ? 0.35f : 1.0f) * power;
    d.velocity = flying + (out + vec3(0.0f, 0.8f, 0.0f)) * speed * std::sqrt(scale);
    d.axis = normalize(vec3(uniform(-1.0f, 1.0f), uniform(-1.0f, 1.0f), uniform(-1.0f, 1.0f)) + vec3(0.0f, 0.01f, 0.0f));
    d.spin = uniform(4.0f, 14.0f);
    d.resting = false;
    debris.push_back(d);
  }
  for (size_t i = 0; i < emitters.size(); i++) {
    emitters[i]->setPosition(centre);
    emitters[i]->setBaseVelocity(vec3(0.0f));
  }
  emitters[0]->burst((int)(70 * power));
  emitters[1]->burst((int)(40 * power));
  emitters[2]->burst((int)(120 * power));
  flashTime = FLASH_TIME;
  // the blast kills the player on foot if he is near
  if (targetPosition && playerCaught && !(playerInVehicle && playerInVehicle()) &&
      !(playerDead && playerDead()) &&
      length(targetPosition() + vec3(0.0f, HEAD_HEIGHT * 0.5f, 0.0f) - centre) < BLAST_RADIUS * scale)
    playerCaught();
  auto bang = soundEngine.play(bangClip(), true, centre);
  if (bang) {
    bang->setVolume(BANG_VOLUME * scale);
    bangSound = std::move(bang);
  }
}

// The pieces fly, spin, fall and bounce on the ground until they lie still
void Mosquito::updateDebris(double dt) {
  float dtf = (float)dt;
  flashTime = std::max(0.0f, flashTime - dtf);
  mat4 toObject = inverse(frozen);
  for (Debris &d : debris) {
    if (!d.resting) {
      d.velocity.y -= 20.0f * dtf;
      vec3 c = vec3(d.world * vec4(d.centre, 1.0f));
      vec3 moved = c + d.velocity * dtf;
      // spin about its own middle, and move
      mat4 turn = glm::translate(mat4(1.0f), moved) * glm::rotate(mat4(1.0f), d.spin * dtf, d.axis) *
                  glm::translate(mat4(1.0f), -c);
      d.world = turn * d.world;
      float ground = groundAt(moved.x, moved.z) + 0.05f * scale;
      if (moved.y < ground) { // a bounce, losing most of it
        d.world = glm::translate(mat4(1.0f), vec3(0.0f, ground - moved.y, 0.0f)) * d.world;
        d.velocity = vec3(d.velocity.x * 0.5f, -d.velocity.y * 0.3f, d.velocity.z * 0.5f);
        d.spin *= 0.5f;
        if (length(d.velocity) < 0.8f)
          d.resting = true;
      }
    }
    setPartTransform(d.part, toObject * d.world);
  }
}

void Mosquito::getLight(vector<SpotLight> &lights) const {
  if (flashTime <= 0.0f)
    return;
  float k = flashTime / FLASH_TIME;
  vec3 centre = vec3(frozen * vec4(0.0f, -0.2f, -0.3f, 1.0f));
  lights.push_back(SpotLight::omni(centre, vec3(1.0f, 0.6f, 0.2f) * (4.0f * k * k), FLASH_RANGE * scale));
}

// It faces where it goes, or the target while it stalks and dives; it leans into its turns and
// tips its head down to dive
void Mosquito::updateAttitude(double dt) {
  float dtf = (float)dt;
  vec3 look = velocity;
  if (behavior == Behavior::Stalk || behavior == Behavior::Dive)
    look = aimPoint() - position;
  else if (behavior == Behavior::TireAttack && vehicle)
    look = vehicle->wheelHub(tireTarget) - position;
  float horizontal = length(vec2(look.x, look.z));
  float wantYaw = horizontal > 0.3f ? std::atan2(look.x, look.z) : yaw;
  vec3 siphonAt;
  float siphonYaw = yaw;
  bool atCap = false;
  if (behavior == Behavior::Siphon && vehicle) { // facing the wall, level (see siphonSpot)
    siphonSpot(siphonAt, siphonYaw);
    atCap = length(siphonAt - position) < 4.0f;
    if (atCap)
      wantYaw = siphonYaw;
  }
  float turnStep = clamp(angleTo(yaw, wantYaw), -TURN_RATE * dtf, TURN_RATE * dtf);
  yaw += turnStep;
  float wantPitch = HOVER_PITCH;
  if (behavior == Behavior::Dive || behavior == Behavior::Stalk || behavior == Behavior::TireAttack)
    wantPitch += clamp(std::atan2(-look.y, std::max(horizontal, 0.1f)), -0.5f, 0.9f) * 0.8f;
  else if (atCap)
    wantPitch = 0.0f;
  else if (behavior == Behavior::Lay)
    wantPitch = LAY_PITCH;
  else
    wantPitch += clamp(length(vec2(velocity.x, velocity.z)) * 0.03f, 0.0f, 0.3f);
  pitch = approach(pitch, wantPitch, 1.5f * dtf);
  float yawRate = dtf > 0.0f ? turnStep / dtf : 0.0f;
  roll = approach(roll, clamp(-yawRate * 0.3f, -0.5f, 0.5f), 1.5f * dtf);
  mat4 r = rotate(mat4(1.0f), yaw, vec3(0.0f, 1.0f, 0.0f));
  r = rotate(r, pitch, vec3(1.0f, 0.0f, 0.0f));
  r = rotate(r, roll, vec3(0.0f, 0.0f, 1.0f));
  rotation = r;
}

// Each wing goes up and down about its hinge and sweeps forward and back a quarter of a beat
// later (a figure of eight); dead, they lie folded back along the body
void Mosquito::updateWings(double dt) {
  float flap = 0.0f, sweep = radians(70.0f);
  if (behavior != Behavior::Dead) {
    float beat = WING_BEAT_HZ * (behavior == Behavior::Dive ? 1.3f : behavior == Behavior::Bite ? 0.4f : 1.0f);
    wingPhase = std::fmod(wingPhase + beat * (float)dt, 1.0f);
    float a = 2.0f * PI * wingPhase;
    flap = radians(WING_RAISE_DEG + WING_FLAP_DEG * std::sin(a));
    sweep = radians(WING_SWEEP_DEG * std::cos(a));
  }
  vec3 hinge(WING_HINGE_X, WING_HINGE_Y, WING_HINGE_Z);
  mat4 left = glm::translate(mat4(1.0f), hinge);
  left = glm::rotate(left, sweep, vec3(0.0f, 1.0f, 0.0f));
  left = glm::rotate(left, flap, vec3(0.0f, 0.0f, 1.0f));
  mat4 right = glm::translate(mat4(1.0f), vec3(-hinge.x, hinge.y, hinge.z));
  right = glm::rotate(right, -sweep, vec3(0.0f, 1.0f, 0.0f));
  right = glm::rotate(right, -flap, vec3(0.0f, 0.0f, 1.0f));
  setPartTransform(wingLeft, left);
  setPartTransform(wingRight, right);
}

// The legs go towards a pose (hanging as they were made, a claw at the player when it is about to
// dive and while it dives, curled up when it is dead) and the physics of InsectLegs makes them
// lag and swing as the body moves
void Mosquito::updateLegs(double dt) {
  float dtf = (float)dt;
  // what the legs reach for: the player's head (the ring closing as it gets there), the wheel it
  // is going to burst, or the wall round the fuel cap, which they cling to (a wide ring)
  bool claw = behavior == Behavior::Dive ||
              (behavior == Behavior::Stalk && diveWait < CLAW_PREPARE);
  vec3 clawTarget = aimPoint();
  if (behavior == Behavior::TireAttack && vehicle) {
    claw = true;
    clawTarget = vehicle->wheelHub(tireTarget);
  } else if (behavior == Behavior::Bite && targetPosition) { // holding on to his head
    claw = true;
    clawTarget = targetPosition() + vec3(0.0f, HEAD_HEIGHT + biteHeight, 0.0f) + biteDir * 0.3f;
  } else if (behavior == Behavior::Siphon && vehicle) {
    vec3 spot, normal;
    float faceYaw;
    siphonSpot(spot, faceYaw);
    vehicle->fuelCap(clawTarget, normal);
    claw = length(spot - position) < 3.0f;
  }
  float ring = mix(CLAW_CLOSED, CLAW_OPEN,
                   clamp((length(clawTarget - position) / scale - BITE_RANGE) / 4.0f, 0.0f, 1.0f));
  if (behavior == Behavior::Siphon)
    ring = CLAW_OPEN;
  else if (behavior == Behavior::Bite)
    ring = 0.45f; // (round his head)
  clawWeight = approach(clawWeight, claw ? 1.0f : 0.0f, (claw ? CLAW_IN_RATE : CLAW_OUT_RATE) * dtf);
  curlWeight = approach(curlWeight, behavior == Behavior::Dead ? 1.0f : 0.0f, 2.0f * dtf);

  const InsectLegs::Pose &hang = legs.getBind();
  InsectLegs::Pose pose = hang, claws, curled;
  if (clawWeight > 0.0f)
    clawPose(claws, clawTarget, ring);
  if (curlWeight > 0.0f)
    curlPose(curled);
  for (size_t l = 0; l < pose.size(); l++)
    for (size_t j = 1; j < pose[l].size(); j++) {
      if (clawWeight > 0.0f)
        pose[l][j] = mix(pose[l][j], claws[l][j], clawWeight);
      if (curlWeight > 0.0f)
        pose[l][j] = mix(pose[l][j], curled[l][j], curlWeight);
    }
  legs.setTargets(pose);
  mat4 bodyToWorld = glm::translate(mat4(1.0f), position) * rotation * glm::scale(mat4(1.0f), vec3(scale));
  legs.setBody(bodyToWorld);
  legs.step(dt);
  vector<mat4> segments;
  legs.segmentTransforms(segments);
  for (size_t i = 0; i < segments.size() && i < legParts.size(); i++)
    setPartTransform(legParts[i], segments[i]);
}

// All the legs reach for `target` (in the body's frame): their feet in a ring `ring` wide round
// it, the front legs above, the hind ones below, the joints bowed outwards, like the fingers of a
// claw.
void Mosquito::clawPose(InsectLegs::Pose &pose, const vec3 &target, float ring) const {
  const InsectLegs::Pose &bind = legs.getBind();
  pose = bind;
  const vec3 centre(0.0f, -0.2f, 0.1f); // the middle of the hips
  vec3 aim = vec3(transpose(rotation) * vec4(target - position, 0.0f)) / scale; // (body units)
  vec3 reach = aim - centre;
  float distance = length(reach);
  vec3 axis = distance > 0.01f ? reach / distance : vec3(0.0f, 0.0f, 1.0f);
  if (axis.z < 0.3f) { // (always somewhere in front of it)
    axis.z = 0.3f;
    axis = normalize(axis);
  }
  vec3 tipsAt = centre + axis * clamp(distance, CLAW_REACH_MIN, CLAW_REACH_MAX);
  vec3 u = cross(vec3(0.0f, 1.0f, 0.0f), axis); // towards its left (+x), across the claw
  u = length(u) > 1e-3f ? normalize(u) : vec3(1.0f, 0.0f, 0.0f);
  vec3 w = cross(axis, u); // up, across the claw
  // where each foot goes round the ring (radians from its left): L0, R0, L1, R1, L2, R2
  const float angles[6] = {1.05f, PI - 1.05f, 0.0f, PI, -1.05f, PI + 1.05f};
  for (size_t l = 0; l < pose.size() && l < 6; l++) {
    vec3 radial = u * std::cos(angles[l]) + w * std::sin(angles[l]);
    vec3 hip = bind[l][0];
    vec3 tip = tipsAt + radial * ring;
    pose[l][1] = hip + axis * 0.25f + radial * 0.35f + w * 0.1f;
    pose[l][2] = mix(hip, tip, 0.45f) + radial * 0.5f;
    pose[l][3] = mix(hip, tip, 0.75f) + radial * 0.35f;
    pose[l][4] = tip;
  }
}

// Dead insects curl their legs under their bodies (in the body's frame: under the belly)
void Mosquito::curlPose(InsectLegs::Pose &pose) const {
  pose = legs.getBind();
  for (auto &leg : pose) {
    vec3 hip = leg[0];
    vec3 out = normalize(vec3(hip.x > 0.0f ? 1.0f : -1.0f, 0.0f, hip.z * 2.0f));
    leg[1] = hip + out * 0.3f + vec3(0.0f, -0.1f, 0.0f);
    leg[2] = hip + out * 0.4f + vec3(0.0f, -0.5f, 0.0f);
    leg[3] = hip + out * 0.1f + vec3(0.0f, -0.75f, 0.0f);
    leg[4] = hip - out * 0.15f + vec3(0.0f, -0.6f, 0.0f);
  }
}

// The buzz follows it, higher and louder the faster it flies
void Mosquito::updateBuzz() {
  if (behavior == Behavior::Dead) {
    buzzSound.reset();
    return;
  }
  if (!buzzSound) {
    buzzSound = soundEngine.playGenerated(buzz, true, position);
    if (!buzzSound)
      return; // no audio
    buzzSound->setVolume(BUZZ_VOLUME);
  }
  float effort = clamp(length(velocity) / DIVE_SPEED, 0.0f, 1.0f);
  // a small one beats its wings faster: higher, and quieter
  buzz->setFrequency(mix(BUZZ_HZ, BUZZ_DIVE_HZ, effort) / std::sqrt(scale));
  buzzSound->setVolume(BUZZ_VOLUME * std::max(scale, 0.15f));
  buzz->setLevel(0.75f + 0.25f * effort);
  buzzSound->setPosition(position);
}

// It falls, then rolls onto its back with its legs up
void Mosquito::updateDead(double dt) {
  float dtf = (float)dt;
  // where the stage left it, this frame and the one before
  bool still = std::fabs(position.y - lastDeadY) < 0.02f;
  lastDeadY = position.y;
  DynamicGameObject::update(dt);
  // (its collision box keeps it a little higher until it has rolled over)
  float rest = groundAt(position.x, position.z) + DEAD_REST_HEIGHT * scale;
  // (or it lies on something: since the last frame, the stage has held it where it was)
  stateTime += dtf;
  if (position.y <= rest + 0.25f || (stateTime > 0.5f && still))
    landed = true;
  if (landed) {
    position.y = std::max(position.y, rest);
    velocity = vec3(0.0f);
    deadRoll = approach(deadRoll, PI, 3.0f * dtf);
  }
  pitch = approach(pitch, 0.0f, 2.0f * dtf);
  mat4 r = rotate(mat4(1.0f), yaw, vec3(0.0f, 1.0f, 0.0f));
  r = rotate(r, pitch, vec3(1.0f, 0.0f, 0.0f));
  r = rotate(r, roll + deadRoll, vec3(0.0f, 0.0f, 1.0f));
  rotation = r;
  updateWings(dt);
  updateLegs(dt);
}

void Mosquito::kill() {
  if (behavior == Behavior::Dead)
    return;
  enterBehavior(Behavior::Dead);
  buzzSound.reset();
  setGravity(GRAVITY);
  setDrag(2.0f);
  acceleration = vec3(0.0f);
  updateWings(0.0);
}

// Only a blow from outside kills it: the part of the change that just stops its own flight (it
// flew into something) does not count, so diving into the player or a rock does not kill it, but
// being run over does (the RV gives it a velocity it did not have)
void Mosquito::applyCollision(const vec3 &push, const vec3 &velocityChange) {
  vec3 before = velocity;
  DynamicGameObject::applyCollision(push, velocityChange);
  vec3 blow = velocityChange;
  float speed = length(before);
  if (speed > 0.01f) {
    vec3 along = before / speed;
    float stopping = clamp(-dot(blow, along), 0.0f, speed);
    blow += along * stopping;
  }
  if (length(blow) > DEATH_SPEED_CHANGE)
    kill();
}

void Mosquito::teleport(const vec3 &where) {
  DynamicGameObject::teleport(where);
  hasWaypoint = false;
  legs.reset();
}

void Mosquito::turn(float radians) { yaw += radians; }

const char *Mosquito::behaviorName() const {
  if (exploded)
    return "reventado";
  return behavior == Behavior::Wander ? (isGrown() ? "busca agua" : "cria: revolotea")
         : behavior == Behavior::ToWater ? "va al agua"
         : behavior == Behavior::Lay ? "pone huevos"
         : behavior == Behavior::Siphon ? (sucking ? "chupa gasolina" : "va a por la gasolina")
         : behavior == Behavior::TireAttack ? "va a pinchar una rueda"
         : behavior == Behavior::Feed ? "va a picarte (sangre)"
         : behavior == Behavior::Bite ? "te esta picando"
         : behavior == Behavior::Stalk ? "acecha (da vueltas al jugador)"
         : behavior == Behavior::Dive ? "se lanza en picado"
         : behavior == Behavior::Retreat ? "se aleja"
                                         : "muerto";
}

void Mosquito::describe(vector<string> &lines) const {
  DynamicGameObject::describe(lines);
  lines.push_back(string("Comportamiento: ") + behaviorName());
  lines.push_back(string("Hora de atacar: ") + (isAttackTime() ? "si" : "no"));
  lines.push_back("Huevos puestos: " + to_string(eggs));
  lines.push_back(textFormat("Gasolina chupada: %.0f %% de un deposito  Estomago: %.0f %% gasolina, %.0f %% sangre%s",
                             fuelTaken * 100.0f, stomach * 100.0f, blood * 100.0f,
                             stomach >= KAMIKAZE_FUEL ? " (EXPLOSIVO)" : ""));
  if (!isGrown())
    lines.push_back(textFormat("Cria: %.0f %% crecida (tamano %.0f %%)", growth * 100.0f, scale * 100.0f));
  if (behavior == Behavior::Stalk)
    lines.push_back(textFormat("Picado en: %.1f s", std::max(diveWait, 0.0f)));
}

void Mosquito::getProperties(vector<Property> &properties) {
  DynamicGameObject::getProperties(properties);
  properties.push_back(Property::info("Comportamiento", [this]() { return string(behaviorName()); }));
  properties.push_back(Property::action("Lanzarse en picado", [this]() {
    if (behavior != Behavior::Dead)
      enterBehavior(Behavior::Dive);
  }));
  properties.push_back(Property::action("Ir a pinchar una rueda", [this]() {
    if (behavior != Behavior::Dead && vehicle) {
      tireTarget = std::uniform_int_distribution<int>(0, 3)(random);
      enterBehavior(Behavior::TireAttack);
    }
  }));
  properties.push_back(Property::number(
      "Estomago (gasolina)", 0.0f, 100.0f, 1.0f, [this]() { return stomach * 100.0f; },
      [this](float v) { setStomach(v / 100.0f); }, "%"));
  properties.push_back(Property::number(
      "Estomago (sangre)", 0.0f, 100.0f, 1.0f, [this]() { return blood * 100.0f; },
      [this](float v) { setBlood(v / 100.0f); }, "%"));
  properties.push_back(Property::number(
      "Crecimiento", 0.0f, 100.0f, 1.0f, [this]() { return growth * 100.0f; },
      [this](float v) { setGrowth(v / 100.0f); }, "%"));
  properties.push_back(Property::action("Explotar", [this]() { explode(); }));
  properties.push_back(Property::action("Matar", [this]() { kill(); }));
}
