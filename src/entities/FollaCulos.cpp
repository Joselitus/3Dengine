#include "FollaCulos.h"
#include "FollaCulosRig.h"

#include <algorithm>
#include <cmath>

using namespace std;
using namespace glm;
using namespace folla_culos_rig;


FollaCulos::FollaCulos(shared_ptr<AnimatedModel> running, shared_ptr<AnimatedModel> splat,
                       SoundEngine &engine, SpeechSynthesizer &synthesizer)
    : Npc(running, "Folla Culos", vector<string>(), engine, synthesizer),
      soundEngine(engine), screechClip(new AudioClip()), random(std::random_device()()),
      ragdoll(bindPoints(), bones(), links()), gait(bindPoints(), bones(), spiderRig()),
      runningModel(running), splatModel(splat) {
  if (!screechClip->loadWavFile("../assets/folla_culos/follaculos_screech.wav"))
    screechClip.reset(); // (no screech: the creature is just quiet)
  running->useRealSize();
  splat->useRealSize();
  addMesh(splat); // mesh 1
  setDrag(0.0f);               // it runs at its own pace (no NPC drag)
  setMaxSpeed(RUN_SPEED);
  setMaxAcceleration(60.0f);
}

// Dead: the eyes stop glowing (and giving off light: see getLight)
void FollaCulos::die() {
  runningModel->setGlowing(false);
  splatModel->setGlowing(false);
}

void FollaCulos::kill() {
  if (dead || stuck || ragdolling)
    return;
  devoured.clear(); // (its prey's head falls from its mouth: see devour)
  running = false;
  screech.reset(); // dying, it stops screeching
  velocity = acceleration = vec3(0.0f);
  setCollidable(false);   // nothing bumps into the body that is not there
  if (frontHit && surfaceFrame && frontHit(*this)) {
    // Run over from the front: it ends up spread out on the windshield, for good
    criticalCondition = true;
    stuck = true;
    setMesh(Splat);
  } else {
    dead = true;
    die();
    setVisible(false);    // not drawn (and with it, its light: see getLight)
  }
}

// It lets go of the windshield and falls: from where its bones are now (as the splat animation
// has them) it becomes a ragdoll, moving as the vehicle was
void FollaCulos::startRagdoll() {
  if (!stuck || !aniModel)
    return;
  vector<vec3> world(POINTS);
  vector<vec3> bind = bindPoints();
  mat3 axes(rotation);
  for (int i = 0; i < POINTS; i++) {
    mat4 bone;
    vec3 local = bind[i];
    if (aniModel->getBoneGlobal(boneOf(i), bone))
      local = vec3(bone * vec4(bind[i] - bind[jointOf(i)], 1.0f));
    world[i] = position + axes * local;
  }
  criticalCondition = false;
  stuck = false;
  dead = true;
  ragdolling = true;
  die();
  setMesh(Running); // the model whose bones the ragdoll poses
  rotation = mat4(1.0f);
  vec3 velocity = carrierVelocity ? carrierVelocity() : vec3(0.0f);
  ragdoll.start(world, velocity);
}

void FollaCulos::applyCollision(const vec3 &push, const vec3 &velocityChange) {
  if (dead || stuck)
    return;
  Npc::applyCollision(push, velocityChange);
  if (length(vec2(velocityChange.x, velocityChange.z)) > DEATH_SPEED_CHANGE)
    kill();
}

void FollaCulos::takeDamage(float amount, const vec3 &direction, const Stage &stage) {
  if (dead || stuck)
    return;
  health -= amount;
  if (health <= 0.0f)
    kill();
}

// While it chases, at any frame it may screech, if it is not screeching already
void FollaCulos::updateScreech(double dt) {
  if (screech && !screech->isPlaying())
    screech.reset();
  if (dead || stuck)
    screech.reset(); // (a dying creature does not chase: it stops, whatever was sounding)
  if (screech) {
    screech->setPosition(position + vec3(0.0f, EYE_HEIGHT, 0.0f)); // it screeches where it runs
    return;
  }
  if (!pursuing || !screechClip)
    return;
  float chance = 1.0f - std::pow(1.0f - SCREECH_CHANCE_PER_SECOND, (float)dt);
  if (std::uniform_real_distribution<float>(0.0f, 1.0f)(random) < chance) {
    screech = soundEngine.play(screechClip, true, position + vec3(0.0f, EYE_HEIGHT, 0.0f));
    if (screech)
      screech->setVolume(SCREECH_VOLUME);
  }
}

// Which behaviour comes next (the transitions of the state machine; see the class comment)
FollaCulos::Behavior FollaCulos::nextBehavior(float distance) const {
  if (retreating || (playerDead && playerDead())) // it caught an NPC or killed the player: it runs away, night or day
    return Behavior::RunAway;
  bool night = !isNight || isNight();
  if (!night)
    return Behavior::RunAway; // the sun is up, whatever it was doing
  if (prey) // (findPrey, asked before: another NPC is near)
    return Behavior::Hunt;
  bool inVehicle = playerInVehicle && playerInVehicle();
  switch (behavior) {
  case Behavior::Hunt: // it caught it, or lost it
  case Behavior::RunAway: // night has fallen
  case Behavior::Pursuit:
    return inVehicle && distance <= CAUTIONARY_RANGE ? Behavior::Caution : Behavior::Pursuit;
  case Behavior::Caution:
    return inVehicle ? Behavior::Caution : Behavior::Pursuit; // the player got out
  }
  return behavior;
}

void FollaCulos::enterBehavior(Behavior next) {
  behavior = next;
  hasCautionGoal = false; // (a new Caution starts by choosing a place)
  cautionPause = 0.0f;
}

// The NPC it hunts: the one it already has while it is not much further than CAUTION_MIN_RADIUS,
// else the nearest one that is closer than that (null if there is none)
Npc *FollaCulos::findPrey() {
  if (!otherNpcs || retreating) // (with its prey in its mouth it hunts no more)
    return nullptr;
  auto distanceTo = [this](const Npc *npc) {
    vec3 d = npc->getPosition() - position;
    return length(vec2(d.x, d.z));
  };
  if (prey && !prey->isRagdolling() && distanceTo(prey) < 1.5f * CAUTION_MIN_RADIUS)
    return prey;
  Npc *best = nullptr;
  float bestDistance = CAUTION_MIN_RADIUS;
  for (Npc *npc : otherNpcs()) {
    if (npc == this || npc->isRagdolling() || !npc->isVisible())
      continue;
    float d = distanceTo(npc);
    if (d < bestDistance) {
      best = npc;
      bestDistance = d;
    }
  }
  return best;
}

// Where its mouth is in the world: its place in the bind pose carried by its head bone's pose
vec3 FollaCulos::mouthPosition() const {
  auto head = lastPose.find("head");
  vec3 local(0.0f, MOUTH_BIND_Y, MOUTH_BIND_Z);
  if (head != lastPose.end())
    local = vec3(head->second * vec4(0.0f, MOUTH_BIND_Y - HEAD_JOINT_Y, MOUTH_BIND_Z - HEAD_JOINT_Z, 1.0f));
  return position + mat3(rotation) * local;
}

// It has caught an NPC: the NPC turns into a ragdoll whose head is held in its mouth
void FollaCulos::devour(Npc &npc) {
  bool done = npc.startRagdoll(floorHeight, [this](vec3 &where) {
    if (dead || stuck || ragdolling) // (if it dies the head falls from its mouth)
      return false;
    where = mouthPosition();
    return true;
  });
  if (done) {
    devoured.push_back(&npc);
    retreating = true; // it takes its prey away: whatever the time of day
  }
}

// Caution: keeps to the edge of the circle of CAUTIONARY_RANGE round `target` (the player) and goes
// round it by the arc, to random points, pausing at each. Returns the velocity it wants.
vec3 FollaCulos::cautionMove(const vec3 &target, double dt) {
  vec2 rel(position.x - target.x, position.z - target.z);
  float r = length(rel);
  if (r < 1e-3f) { // (right on the player: any way out)
    rel = vec2(1.0f, 0.0f);
    r = 1.0f;
  }
  float angle = atan2(rel.y, rel.x);
  vec2 outward = rel / r, tangent(-sin(angle), cos(angle)); // (the way round that grows the angle)
  float error = CAUTIONARY_RANGE - r;                      // > 0: inside the circle, < 0: outside it
  bool onEdge = std::fabs(error) <= CAUTION_EDGE_TOLERANCE;
  // always pulled to the edge: out if it is inside, in if outside (running when it is far off)
  float radial = glm::clamp(error * 3.0f, -RUN_SPEED, RUN_SPEED);
  vec2 move = outward * radial;
  running = true;

  if (cautionPause > 0.0f && onEdge) {
    cautionPause -= (float)dt; // standing, watching the player
    faceTowards(vec3(target.x, position.y, target.z));
    running = false;
    return vec3(move.x, 0.0f, move.y);
  }
  if (!hasCautionGoal) { // a point of the edge a random way round
    uniform_real_distribution<float> unit(0.0f, 1.0f);
    float arc = CAUTION_ARC_MIN + unit(random) * (CAUTION_ARC_MAX - CAUTION_ARC_MIN);
    cautionAngle = angle + (unit(random) < 0.5f ? -arc : arc);
    hasCautionGoal = true;
  }
  float turn = atan2(sin(cautionAngle - angle), cos(cautionAngle - angle)); // the short way round
  if (std::fabs(turn) * CAUTIONARY_RANGE < 0.5f) { // arrived: it stops a moment
    hasCautionGoal = false;
    cautionPause = uniform_real_distribution<float>(CAUTION_PAUSE_MIN, CAUTION_PAUSE_MAX)(random);
    running = false;
    return vec3(move.x, 0.0f, move.y);
  }
  if (onEdge) // (far from the edge it goes back to it first, then goes round)
    move += tangent * (turn > 0.0f ? CAUTION_WALK_SPEED : -CAUTION_WALK_SPEED);
  float speed = length(move);
  if (speed > 1e-3f)
    faceTowards(position + vec3(move.x, 0.0f, move.y) / speed);
  return vec3(move.x, 0.0f, move.y);
}

void FollaCulos::update(double dt) {
  if (dead || stuck) {
    if (stuck && surfaceFrame) {
      // It follows the windshield: its chest on the glass, facing it (its front, +z, goes into
      // the glass) and its head up the slope
      vec3 center, up, normal;
      surfaceFrame(center, up, normal);
      vec3 into = -normal;
      mat3 axes(cross(up, into), up, into);
      float roll = radians(STUCK_ROLL);
      axes = axes * mat3(vec3(cos(roll), sin(roll), 0.0f), vec3(-sin(roll), cos(roll), 0.0f),
                         vec3(0.0f, 0.0f, 1.0f)); // turned about its own z (the normal)
      position = center + normal * STUCK_OFFSET - axes * vec3(0.0f, ANCHOR_HEIGHT, 0.0f);
      rotation = mat4(axes);
      splatClock += dt;
      if (aniModel)
        aniModel->Update(splatClock);
      // The vehicle slows down: it lets go (a copy waits to be told)
      if (carrierVelocity && !replica) {
        vec3 v = carrierVelocity();
        if (length(vec2(v.x, v.z)) < RAGDOLL_SPEED)
          startRagdoll();
      }
    }
    if (ragdolling) {
      ragdoll.step(dt, floorHeight, pushOut);
      position = ragdoll.getPoints()[PELVIS];
      map<string, mat4> pose;
      ragdoll.boneGlobals(position, pose);
      if (aniModel)
        aniModel->setBoneGlobals(pose);
    }
    return; // (otherwise it stays where it died)
  }
  if (replica) { // (the server thinks for it: here it only walks, and screeches)
    animate(dt);
    return;
  }
  // The state machine: which behaviour now, and what it does
  vec3 wanted(0.0f);
  running = false;
  pursuing = false;
  if (targetPosition) {
    vec3 target = targetPosition();
    vec3 d = target - position;
    d.y = 0.0f;
    float distance = length(d);
    if (retreating && distance >= FLEE_DISTANCE) // far enough: the retreat is over
      retreating = false;
    prey = findPrey();
    Behavior next = nextBehavior(distance);
    if (next != behavior)
      enterBehavior(next);
    switch (behavior) {
    case Behavior::Pursuit:
      // it goes for the target
      if (playerCaught && distance <= PLAYER_CATCH_DISTANCE && !(playerInVehicle && playerInVehicle()) &&
          !(playerDead && playerDead())) {
        playerCaught(); // it has touched the player on foot: he dies, and it runs away
        retreating = true;
      } else if (distance > STOP_DISTANCE && distance > 1e-3f) {
        vec3 direction = d / distance;
        wanted = direction * RUN_SPEED;
        faceTowards(position + direction); // yaw 0 is towards +z, like the Walker
        running = true;
        pursuing = true;
      }
      break;
    case Behavior::RunAway:
      // it goes away from the target
      if (distance < FLEE_DISTANCE && distance > 1e-3f) {
        vec3 direction = -d / distance;
        wanted = direction * RUN_SPEED;
        faceTowards(position + direction);
        running = true;
      }
      break;
    case Behavior::Caution:
      wanted = cautionMove(target, dt);
      break;
    case Behavior::Hunt:
      if (prey) {
        vec3 toPrey = prey->getPosition() - position;
        toPrey.y = 0.0f;
        float far = length(toPrey);
        if (far <= CATCH_DISTANCE) {
          devour(*prey); // they collide: it is its now
          prey = nullptr;
        } else {
          wanted = toPrey / far * RUN_SPEED;
          faceTowards(position + toPrey / far);
          running = true;
          pursuing = true;
        }
      }
      break;
    }
  }
  wanted.y = velocity.y; // (falling is not steered)
  steerTowards(wanted, 12.0f);
  animate(dt);
}

void FollaCulos::animate(double dt) {
  DynamicGameObject::update(dt);
  updateScreech(dt);
  // The pose: made by the gait from where its body is (its feet stay where they landed)
  // (how fast it moves is measured from where it was, not read from `velocity`: a drag, a push
  // or a teleport moves it too, and the feet have to follow whatever moved it)
  if (!hasLastPosition) {
    lastPosition = position;
    hasLastPosition = true;
  }
  if (dt > 0.0) {
    vec3 moved = (position - lastPosition) / (float)dt;
    moved.y = 0.0f;
    float speed = length(moved);
    if (speed > 2.0f * RUN_SPEED)
      moved *= 2.0f * RUN_SPEED / speed;
    gaitVelocity += (moved - gaitVelocity) * glm::min(1.0f, (float)dt * 15.0f);
  }
  lastPosition = position;
  gait.setBody(position, mat3(rotation), gaitVelocity);
  if (lookTarget)
    gait.setLookTarget(lookTarget());
  if (floorHeight)
    gait.setFloor(floorHeight);
  gait.step(dt);
  if (aniModel) {
    map<string, mat4> pose;
    gait.boneGlobals(vec3(0.0f), pose);
    aniModel->setBoneGlobals(pose);
    lastPose = pose;
  }
}

void FollaCulos::getLight(vector<SpotLight> &lights) const {
  if (!visible)
    return;
  if (dead) // (dead eyes do not shine)
    return;
  // In front of its face: running, a bit ahead of the eyes; stuck on the glass, where its head is
  vec3 local = stuck ? vec3(0.0f, 2.17f, 0.15f) : vec3(0.0f, EYE_HEIGHT, 0.5f);
  lights.push_back(SpotLight::omni(position + vec3(rotation * vec4(local, 0.0f)),
                                   vec3(0.55f, 0.42f, 0.02f), EYE_LIGHT_RANGE));
}

void FollaCulos::describe(vector<string> &lines) const {
  Npc::describe(lines);
  lines.push_back(string("Muerto: ") + (!dead ? "no" : ragdolling ? "SI (ragdoll)" : "SI (no se dibuja)"));
  if (stuck)
    lines.push_back("Pegado al parabrisas, agonizando");
  lines.push_back(string("Estado critico: ") + (criticalCondition ? "SI" : "no"));
  const char *state = behavior == Behavior::Pursuit ? "persecucion"
                      : behavior == Behavior::Hunt ? "caza (va a por otro NPC)"
                      : behavior == Behavior::Caution ? "cautela (rodea al jugador por el borde del circulo)"
                                                       : retreating ? "retirada (con su presa)" : "huida";
  lines.push_back(string("Comportamiento: ") + state);
  lines.push_back("Cabezas en la boca: " + to_string(devoured.size()));
  lines.push_back(string("En movimiento: ") + (running ? "si" : "no"));
}

void FollaCulos::writeNetState(NetWriter &out) const {
  Npc::writeNetState(out);
  uint8_t flags = (dead ? 1 : 0) | (stuck ? 2 : 0) | (criticalCondition ? 4 : 0) | (ragdolling ? 8 : 0) |
                  (running ? 16 : 0) | (pursuing ? 32 : 0);
  out.u8(flags);
  out.u8((uint8_t)std::min<size_t>(devoured.size(), 255));
  for (size_t i = 0; i < devoured.size() && i < 255; i++)
    out.i32(devoured[i]->getNetId());
}

void FollaCulos::readNetState(NetReader &in) {
  Npc::readNetState(in);
  uint8_t flags = in.u8();
  uint8_t eaten = in.u8();
  std::vector<int> ids;
  for (int i = 0; i < eaten; i++)
    ids.push_back(in.i32());
  if (!in.isOk())
    return;
  running = flags & 16;
  pursuing = flags & 32;
  // The ones it carries in its mouth
  for (int id : ids) {
    Npc *npc = npcLookup ? npcLookup(id) : nullptr;
    if (npc && std::find(devoured.begin(), devoured.end(), npc) == devoured.end() && !dead && !stuck)
      devour(*npc);
  }
  // What happened to it: stuck on a windshield, dead, a ragdoll (the pieces it does itself)
  bool wasDead = dead || stuck || ragdolling;
  if ((flags & 2) && !stuck && !ragdolling) {
    devoured.clear();
    running = false;
    screech.reset();
    velocity = acceleration = vec3(0.0f);
    setCollidable(false);
    criticalCondition = true;
    stuck = true;
    setMesh(Splat);
  }
  if ((flags & 8) && !ragdolling) {
    if (!stuck && !dead) { // (it was never seen stuck here: it starts from the splat pose)
      stuck = true;
      setMesh(Splat);
    }
    if (stuck)
      startRagdoll();
  } else if ((flags & 1) && !(flags & 2) && !(flags & 8) && !dead && !stuck && !ragdolling) {
    if (!wasDead) {
      devoured.clear();
      running = false;
      screech.reset();
      setCollidable(false);
      dead = true;
      die();
      setVisible(false);
    }
  }
}
