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
// Where the smoke comes out when it touches down, and which way (its frame): under its three feet
// (generate_saucer.py: at 3.5 m from the middle, 30 + 120 k degrees from +x), and from the seams
// of the closed ramp (both sides and its far end)
struct Vent {
  vec3 at, towards;
};
const float FOOT_DISTANCE = 3.5f;
Vent ventAt(int i) {
  if (i < 3) {
    float a = 2.0f * PI * i / 3.0f + PI / 6.0f;
    vec3 out(std::cos(a), 0.0f, std::sin(a));
    return {out * FOOT_DISTANCE + vec3(0.0f, 0.12f, 0.0f), out + vec3(0.0f, 0.25f, 0.0f)};
  }
  const float y = Saucer::RAMP_HINGE_Y - 0.08f, mid = Saucer::RAMP_HINGE_Z + Saucer::RAMP_LENGTH * 0.5f;
  if (i == 3)
    return {vec3(-0.62f, y, mid), vec3(-1.0f, -0.35f, 0.0f)};
  if (i == 4)
    return {vec3(0.62f, y, mid), vec3(1.0f, -0.35f, 0.0f)};
  return {vec3(0.0f, y, Saucer::RAMP_HINGE_Z + Saucer::RAMP_LENGTH), vec3(0.0f, -0.35f, 1.0f)};
}
const int VENTS = 6;
// The ray gun (generate_saucer.py): the end of its barrel, in the gun's frame, and its lowest point
// (the ball under the hull, not the barrel) under the pivot
const vec3 MUZZLE(0.0f, Saucer::GUN_PIVOT_Y - 0.3f, 1.62f);
const float GUN_BOTTOM = 0.42f;
const vec3 SHOT_COLOR(0.45f, 1.0f, 0.55f);

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
               shared_ptr<Model> ramp, shared_ptr<Model> beam, shared_ptr<Model> gunMount,
               shared_ptr<Model> gun, shared_ptr<Model> shot)
    : PlayableCharacter(hull, make_shared<Box>(HULL_HALF, HULL_CENTER)), random(random_device()()) {
  hullPart = 0;
  lightsPart = addPart(lights, 2); // glowing
  legsPart = addPart(legs);
  rampPart = addPart(ramp);
  beamPart = addPart(beam, 2);
  gunMountPart = addPart(gunMount);
  gunPart = addPart(gun);
  shotPart = addPart(shot, 2);
  setPartVisible(gunMountPart, false);
  setPartVisible(gunPart, false);
  setPartVisible(shotPart, false);
  // steam: white puffs that spread, slow down and rise a little as they fade
  ParticleSettings steamPuff;
  steamPuff.lifeMin = 1.6f;
  steamPuff.lifeMax = 2.8f;
  steamPuff.speedMin = 1.5f;
  steamPuff.speedMax = 3.5f;
  steamPuff.spread = 0.5f;
  steamPuff.sizeStart = 0.25f;
  steamPuff.sizeEnd = 1.4f;
  steamPuff.color = vec3(0.86f, 0.88f, 0.9f);
  steamPuff.alpha = 0.4f;
  steamPuff.fadeStart = 0.3f;
  steamPuff.gravity = -0.5f;
  steamPuff.drag = 1.6f;
  steamPuff.maxParticles = 150;
  for (int i = 0; i < VENTS; i++)
    smoke.push_back(make_shared<ParticleEmitter>(steamPuff, 300 + i));
  // sparks where a shot hits: small green specks thrown back, that fall
  ParticleSettings spark;
  spark.lifeMin = 0.25f;
  spark.lifeMax = 0.5f;
  spark.speedMin = 2.0f;
  spark.speedMax = 6.0f;
  spark.spread = 1.1f;
  spark.sizeStart = 0.07f;
  spark.sizeEnd = 0.02f;
  spark.color = vec3(0.6f, 1.0f, 0.65f);
  spark.alpha = 0.95f;
  spark.gravity = 6.0f;
  spark.drag = 1.0f;
  spark.maxParticles = 120;
  sparks = make_shared<ParticleEmitter>(spark, 310);
  emitters = smoke;
  emitters.push_back(sparks);
  setGravity(0.0f);
  setMass(1e6f); // nothing pushes it
  setVisible(false);
  setCollidable(false);
}

void Saucer::setSounds(SoundEngine &engine) {
  soundEngine = &engine;
  auto load = [](const char *file) {
    auto clip = make_shared<AudioClip>();
    if (!clip->loadWavFile(string("../assets/bob/") + file))
      clip.reset();
    return clip;
  };
  humClip = load("saucer_hum.wav");
  powerDownClip = load("saucer_power_down.wav");
  steamClip = load("saucer_steam.wav");
  shotClip = load("saucer_shot.wav");
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
    if (soundEngine && powerDownClip && (powerDown = soundEngine->play(powerDownClip, true, position))) {
      powerDown->setVolume(POWER_DOWN_VOLUME);
      powerDown->setDistances(SOUND_NEAR, 200.0f);
    }
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
    gunWanted = false;
    aiming = false;
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

void Saucer::toggleGun() {
  if (phase == Phase::Piloted)
    gunWanted = !gunWanted;
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
  if (replica) {
    updateReplica(dt);
    return;
  }
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
      place();
      touchDown();
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
  updateGun(dt);
  place(); // (its turn, legs, ramp, beam and gun, as they are now)
  updateSounds(dt);
  updateSmoke(dt);
}

// The gun comes down or goes up; once it is down the camera goes to it (and back behind the ship
// as soon as it goes up); the gun turns to where the camera looks
void Saucer::updateGun(double dt) {
  float dtf = (float)dt;
  bool out = gunWanted && phase == Phase::Piloted;
  gunOut = approach(gunOut, out ? 1.0f : 0.0f, dtf / GUN_TIME);
  bool aim = out && gunOut >= 1.0f;
  if (aim != aiming && camera) {
    if (aim)
      camera->attachTo(this, 0.0f, GUN_PIVOT_Y);
    else
      camera->attachTo(this, CAMERA_DISTANCE, CAMERA_HEIGHT);
  }
  aiming = aim;
  // the barrel turns about the pivot, to look where the pilot looks (in the ship's frame)
  if (aiming) {
    vec3 f = transpose(mat3(rotation)) * (camera ? camera->getForward() : aimForward);
    setBarrel(std::atan2(f.x, f.z), std::asin(clamp(-f.y, -1.0f, 1.0f)));
  }
  shotCooldown = std::max(0.0f, shotCooldown - dtf);
  if (shotTime >= 0.0f) {
    shotTime += dtf;
    if (shotTime > 2.0f * SHOT_SHOW)
      shotTime = -1.0f;
  }
}

void Saucer::setBarrel(float yawL, float pitchL) {
  gunYaw = yawL;
  gunPitch = pitchL;
  vec3 pivot(0.0f, GUN_PIVOT_Y, 0.0f);
  gunLocal = glm::translate(mat4(1.0f), pivot) * glm::rotate(mat4(1.0f), yawL, vec3(0.0f, 1.0f, 0.0f)) *
             glm::rotate(mat4(1.0f), pitchL, vec3(1.0f, 0.0f, 0.0f)) * glm::translate(mat4(1.0f), -pivot);
}

vec3 Saucer::muzzle() const {
  vec3 slide(0.0f, (1.0f - ease(gunOut)) * GUN_TRAVEL, 0.0f);
  return position + vec3(rotation * vec4(vec3(gunLocal * vec4(MUZZLE, 1.0f)) + slide, 0.0f));
}

bool Saucer::fire(const Stage &stage, const vec3 &eye, const vec3 &direction) {
  if (!aiming || shotCooldown > 0.0f || length(direction) < 1e-4f)
    return false;
  shotCooldown = SHOT_COOLDOWN;
  vec3 dir = normalize(direction);
  // the first solid thing along it (not the ship itself, nor what can't be seen: the player in it)
  float best = SHOT_RANGE;
  shared_ptr<GameObject> hit;
  auto consider = [&](const shared_ptr<GameObject> &object) {
    if (object.get() == static_cast<const GameObject *>(this) || !object->isVisible() || !object->isCollidable())
      return;
    float distance;
    if (object->getShape().raycast(object->getPose(), eye, dir, distance) && distance < best) {
      best = distance;
      hit = object;
    }
  };
  for (const auto &object : stage.getObjects())
    consider(object);
  for (const auto &object : stage.getDynamicObjects())
    consider(object);
  // or the ground, if that comes first
  for (float t = 0.25f; t < best; t += 0.25f) {
    vec3 p = eye + dir * t;
    float ground;
    if (stage.floorAt(p.x, p.z, ground) && p.y <= ground) {
      best = t;
      hit.reset();
      break;
    }
  }
  shotFrom = muzzle();
  shotTo = eye + dir * best;
  shotTime = 0.0f;
  shots++;
  if (hit)
    hit->takeDamage(SHOT_DAMAGE, dir, stage);
  sparks->setPosition(shotTo - dir * 0.1f);
  sparks->setDirection(-dir);
  sparks->burst(30);
  if (soundEngine && shotClip && (shotSound = soundEngine->play(shotClip, true, muzzle())))
    shotSound->setVolume(SHOT_VOLUME);
  return true;
}

// It stands on its legs on the ground: a release of steam, and smoke from its feet and its ramp
void Saucer::touchDown() {
  touchdowns++;
  smokeTime = 0.0f;
  updateSmoke(0.0);
  for (auto &emitter : smoke)
    emitter->burst(8);
  if (soundEngine && steamClip && (steam = soundEngine->play(steamClip, true, position))) {
    steam->setVolume(STEAM_VOLUME);
    steam->setDistances(SOUND_NEAR, 200.0f);
  }
}

void Saucer::updateSmoke(double dt) {
  float rate = 0.0f;
  if (smokeTime >= 0.0f) {
    float left = 1.0f - smokeTime / SMOKE_TIME;
    rate = left > 0.0f ? SMOKE_RATE * left * left : 0.0f;
    smokeTime = left > 0.0f ? smokeTime + (float)dt : -1.0f;
  }
  for (int i = 0; i < VENTS; i++) {
    Vent vent = ventAt(i);
    smoke[i]->setPosition(position + vec3(rotation * vec4(vent.at, 0.0f)));
    smoke[i]->setDirection(vec3(rotation * vec4(vent.towards, 0.0f)));
    smoke[i]->setRate(rate);
  }
}

// The hum while it moves (it dies away as it comes down to land) and where its sounds come from
void Saucer::updateSounds(double dt) {
  if (!soundEngine)
    return;
  float wanted = 0.0f, pitch = 1.0f;
  float speed = std::min(length(velocity) / FLY_SPEED, 1.0f);
  switch (phase) {
  case Phase::Arriving:
  case Phase::Leaving:
    wanted = 1.0f;
    break;
  case Phase::Ascending:
    wanted = ease(phaseTime);
    break;
  case Phase::Descending:
    wanted = 1.0f - ease(phaseTime / (DESCEND_TIME * 0.5f));
    break;
  case Phase::Piloted:
    wanted = engineOn ? 0.55f + 0.45f * speed : 0.0f;
    pitch = 0.9f + 0.3f * speed;
    break;
  default:
    break;
  }
  humLevel = approach(humLevel, wanted, 2.0f * (float)dt);
  if (humLevel > 0.01f && humClip) {
    if (!hum && (hum = soundEngine->play(humClip, true, position, true)))
      hum->setDistances(SOUND_NEAR, 200.0f);
    if (hum) {
      hum->setPosition(position);
      hum->setVolume(HUM_VOLUME * humLevel);
      hum->setPitch(pitch);
    }
  } else {
    hum.reset();
  }
  if (powerDown && !powerDown->isPlaying())
    powerDown.reset();
  if (powerDown)
    powerDown->setPosition(position);
  if (steam && !steam->isPlaying())
    steam.reset();
  if (steam)
    steam->setPosition(position);
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
  // (and with its gun down, never below it)
  float gunLowest = GUN_PIVOT_Y - GUN_BOTTOM + (1.0f - ease(gunOut)) * GUN_TRAVEL;
  lowest = std::max(lowest, ground - gunLowest + 0.02f);
  if (position.y <= lowest) {
    position.y = lowest;
    if (velocity.y < 0.0f)
      velocity.y = 0.0f;
  }
  bool wasOnGround = onGround;
  onGround = position.y <= lowest + 0.05f;
  if (onGround && !wasOnGround && legsOut >= 0.99f)
    touchDown();
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
  if (!replica) // (a copy is turned by what the server says)
    rotation = rotate(mat4(1.0f), spin, vec3(0.0f, 1.0f, 0.0f));
  // the legs slide up into the hull; the ramp turns down about its hinge
  setPartVisible(legsPart, legsOut > 0.02f);
  setPartTransform(legsPart, glm::translate(mat4(1.0f), vec3(0.0f, (1.0f - legsOut) * (LEG_HEIGHT - 0.1f), 0.0f)));
  vec3 hinge(0.0f, RAMP_HINGE_Y, RAMP_HINGE_Z);
  mat4 ramp = glm::translate(mat4(1.0f), hinge);
  ramp = rotate(ramp, ease(rampOpen) * RAMP_ANGLE, vec3(1.0f, 0.0f, 0.0f));
  setPartTransform(rampPart, glm::translate(ramp, -hinge));
  setPartVisible(beamPart, beamOn());
  // the gun slides down from the hull, and its barrel turns
  mat4 slide = glm::translate(mat4(1.0f), vec3(0.0f, (1.0f - ease(gunOut)) * GUN_TRAVEL, 0.0f));
  setPartVisible(gunMountPart, gunOut > 0.01f);
  setPartVisible(gunPart, gunOut > 0.01f);
  setPartTransform(gunMountPart, slide);
  setPartTransform(gunPart, slide * gunLocal);
  // the shot: a rod stretched from where the muzzle was to where it hit, both fixed in the world
  // (a beam of light: turning the gun or the ship after it does not drag it along)
  bool shooting = shotTime >= 0.0f && shotTime <= SHOT_SHOW;
  setPartVisible(shotPart, shooting);
  if (shooting) {
    mat3 toShip = transpose(mat3(rotation));
    vec3 from = toShip * (shotFrom - position), to = toShip * (shotTo - position);
    vec3 d = to - from;
    float len = length(d);
    if (len > 0.05f) {
      vec3 z = d / len;
      vec3 x = cross(vec3(0.0f, 1.0f, 0.0f), z);
      x = length(x) > 1e-4f ? normalize(x) : vec3(1.0f, 0.0f, 0.0f);
      mat4 m(1.0f);
      m[0] = vec4(x, 0.0f);
      m[1] = vec4(cross(z, x), 0.0f);
      m[2] = vec4(z * len, 0.0f); // (the rod is 1 m long)
      m[3] = vec4(from, 1.0f);
      setPartTransform(shotPart, m);
    } else {
      setPartVisible(shotPart, false);
    }
  }
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
  if (shotTime >= 0.0f) { // the flash where a shot hits (it lights its sparks too)
    float k = 1.0f - shotTime / (2.0f * SHOT_SHOW);
    lights.push_back(SpotLight::omni(shotTo - normalize(shotTo - shotFrom) * 0.3f, SHOT_COLOR * 3.0f * k, 7.0f));
  }
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

// What the server tells the clients about the ship (where it is and how it is turned go with every
// object): what it is doing, how far its legs, ramp and gun are out, where the gun points, and a
// count of touchdowns and shots, so that each client makes the smoke and sparks of every one
void Saucer::writeNetState(NetWriter &out) const {
  out.u8((uint8_t)phase);
  out.f32(phaseTime);
  out.f32(legsOut);
  out.f32(rampOpen);
  out.f32(gunOut);
  out.f32(gunYaw);
  out.f32(gunPitch);
  out.boolean(taking);
  out.boolean(engineOn);
  out.boolean(onGround);
  out.u32(touchdowns);
  out.u32(shots);
  out.vec3(shotFrom);
  out.vec3(shotTo);
}

void Saucer::readNetState(NetReader &in) {
  uint8_t p = in.u8();
  float time = in.f32();
  legsOutWanted = in.f32();
  rampOpenWanted = in.f32();
  gunOutWanted = in.f32();
  gunYaw = in.f32();
  gunPitch = in.f32();
  taking = in.boolean();
  engineOn = in.boolean();
  onGround = in.boolean();
  unsigned landed = in.u32(), fired = in.u32();
  vec3 from = in.vec3();
  vec3 hit = in.vec3();
  if (!in.isOk() || p > (uint8_t)Phase::Leaving)
    return;
  if ((Phase)p != phase) {
    phase = (Phase)p;
    phaseTime = time;
    if (phase == Phase::Descending && soundEngine && powerDownClip &&
        (powerDown = soundEngine->play(powerDownClip, true, position))) {
      powerDown->setVolume(POWER_DOWN_VOLUME);
      powerDown->setDistances(SOUND_NEAR, 200.0f);
    }
  }
  if (!netPrimed) { // (what happened before this client came is not done again)
    netPrimed = true;
    touchdowns = landed;
    shots = fired;
  }
  if (landed != touchdowns) {
    touchDown();
    touchdowns = landed;
  }
  if (fired != shots) {
    shots = fired;
    shotFrom = from;
    shotTo = hit;
    shotTime = 0.0f;
    sparks->setPosition(shotTo);
    sparks->setDirection(vec3(0.0f, 1.0f, 0.0f));
    sparks->burst(30);
    if (soundEngine && shotClip && (shotSound = soundEngine->play(shotClip, true, muzzle())))
      shotSound->setVolume(SHOT_VOLUME);
  }
}

// The copy on a client: its parts follow what the server says (smoothly), and the rest, the
// sounds, the smoke, the shot's rod, is done here as on the server
void Saucer::updateReplica(double dt) {
  GameObject::update(dt);
  float dtf = (float)dt;
  phaseTime += dtf;
  float follow = std::min(1.0f, 12.0f * dtf);
  legsOut += (legsOutWanted - legsOut) * follow;
  rampOpen += (rampOpenWanted - rampOpen) * follow;
  gunOut += (gunOutWanted - gunOut) * follow;
  bool aim = gunOut >= 0.99f && phase == Phase::Piloted;
  if (aim != aiming && camera) {
    if (aim)
      camera->attachTo(this, 0.0f, GUN_PIVOT_Y);
    else
      camera->attachTo(this, CAMERA_DISTANCE, CAMERA_HEIGHT);
  }
  aiming = aim;
  setBarrel(gunYaw, gunPitch);
  if (shotTime >= 0.0f) {
    shotTime += dtf;
    if (shotTime > 2.0f * SHOT_SHOW)
      shotTime = -1.0f;
  }
  place();
  updateSounds(dt);
  updateSmoke(dt);
}
