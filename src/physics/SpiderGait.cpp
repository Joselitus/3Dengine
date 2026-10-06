#include "SpiderGait.h"

#include <algorithm>
#include <cmath>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace glm;

namespace {
const float PI = 3.14159265f;

// The middle joint of a two-bone limb from `root` to `end` (the lengths of its bones), bent
// towards `pole`
vec3 twoBoneMiddle(const vec3 &root, const vec3 &end, float l1, float l2, const vec3 &pole) {
  vec3 d = end - root;
  float dist = std::max(length(d), 1e-4f);
  vec3 dir = d / dist;
  float a = (l1 * l1 - l2 * l2 + dist * dist) / (2.0f * dist);
  float h = std::sqrt(std::max(l1 * l1 - a * a, 0.0f));
  vec3 n = pole - dir * dot(pole, dir);
  if (length(n) < 1e-4f)
    n = cross(dir, abs(dir.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0));
  return root + dir * a + normalize(n) * h;
}

float horizontal(const vec3 &a, const vec3 &b) {
  return length(vec2(a.x - b.x, a.z - b.z));
}
} // namespace

SpiderGait::SpiderGait(const std::vector<vec3> &bindPoints, const std::vector<RigBone> &bones,
                       const SpiderRig &rig, const SpiderParams &params)
    : bind(bindPoints), points(bindPoints), bones(bones), rig(rig), params(params),
      legs(rig.legs.size()) {}

float SpiderGait::groundAt(const vec3 &world) const {
  float height = position.y;
  if (floor)
    floor(world.x, world.z, height);
  return height;
}

// Where a foot would like to be now: its rest place from the body, on the ground
vec3 SpiderGait::restPlace(const SpiderLeg &leg) const {
  float x = leg.front ? params.frontRestX : params.hindRestX;
  float z = leg.front ? params.frontRestZ : params.hindRestZ;
  vec3 world = position + axes * vec3(leg.side * x, 0.0f, z);
  world.y = groundAt(world) + (leg.front ? params.frontTipHeight : params.tipHeight);
  return world;
}

// A foot landing shakes the body: the end it lands on dips, and the side
void SpiderGait::landed(size_t i, float speed) {
  const SpiderLeg &leg = rig.legs[i];
  float a = 0.35f + 0.65f * std::min(1.0f, speed / 3.0f);
  bob.v -= params.bobKick * a;
  pitch.v += (leg.front ? 1.0f : -0.7f) * params.pitchKick * a; // (the front ones hit harder)
  roll.v -= leg.side * params.rollKick * a;
  head.v += (leg.front ? -1.0f : 1.0f) * params.headKick * a; // the head lags behind the torso
}

// ...and lifting one eases that weight off
void SpiderGait::lifted(size_t i, float speed) {
  const SpiderLeg &leg = rig.legs[i];
  float a = 0.35f + 0.65f * std::min(1.0f, speed / 3.0f);
  bob.v += 0.3f * params.bobKick * a;
  roll.v += leg.side * 0.5f * params.rollKick * a;
  head.v += (leg.front ? 0.3f : -0.3f) * params.headKick * a;
}

void SpiderGait::step(double seconds) {
  float dt = std::min((float)seconds, 0.05f);
  if (dt <= 0.0f)
    return;
  vec3 flat(velocity.x, 0.0f, velocity.z);
  float speed = length(flat);

  if (!placed) {
    for (size_t i = 0; i < legs.size(); i++) {
      legs[i].foot = restPlace(rig.legs[i]);
      legs[i].swinging = false;
    }
    placed = true;
  }
  // The feet in the air go on with their arc (they land a bit ahead of where the body is going)
  for (size_t i = 0; i < legs.size(); i++) {
    Leg &l = legs[i];
    if (!l.swinging)
      continue;
    l.t += dt / l.duration;
    vec3 target = restPlace(rig.legs[i]) + flat * params.leadTime;
    target.y = groundAt(target) + (rig.legs[i].front ? params.frontTipHeight : params.tipHeight);
    if (l.t >= 1.0f) {
      l.foot = target;
      l.swinging = false;
      landed(i, speed);
    } else {
      float s = l.t * l.t * (3.0f - 2.0f * l.t);
      vec3 p = mix(l.from, target, s);
      p.y = mix(l.from.y, target.y, s) + params.liftHeight * std::sin(PI * l.t);
      l.foot = p;
    }
  }
  // The feet the body has gone too far past lift, the farthest first
  float threshold = params.idleThreshold +
                    (params.stepThreshold - params.idleThreshold) * std::min(1.0f, speed / 2.0f);
  std::vector<std::pair<float, size_t>> late;
  for (size_t i = 0; i < legs.size(); i++)
    if (!legs[i].swinging) {
      float lag = horizontal(legs[i].foot, restPlace(rig.legs[i]));
      if (lag > threshold)
        late.push_back(std::make_pair(lag, i));
    }
  std::sort(late.begin(), late.end(), [](const std::pair<float, size_t> &a,
                                         const std::pair<float, size_t> &b) { return a.first > b.first; });
  for (const auto &entry : late) {
    size_t i = entry.second;
    const SpiderLeg &leg = rig.legs[i];
    int pair = ((leg.front ? 1 : 0) + (leg.side > 0.0f ? 0 : 1)) % 2; // diagonals go together
    bool otherPair = false;
    for (size_t j = 0; j < legs.size(); j++)
      if (legs[j].swinging) {
        const SpiderLeg &o = rig.legs[j];
        if (((o.front ? 1 : 0) + (o.side > 0.0f ? 0 : 1)) % 2 != pair)
          otherPair = true;
      }
    bool desperate = entry.first > threshold * 1.8f + 0.05f; // too far behind: no waiting
    if (otherPair && !desperate)
      continue;
    Leg &l = legs[i];
    l.swinging = true;
    l.t = 0.0f;
    l.from = l.foot;
    float stride = 2.0f * threshold + speed * params.leadTime;
    l.duration = glm::clamp(0.45f * stride / std::max(speed, 0.4f), 0.09f, 0.28f);
    if (!leg.front)
      l.duration *= 1.2f; // the hind feet land a little after the front ones of their pair
    lifted(i, speed);
  }

  bob.step(dt, 30.0f, 0.22f);
  pitch.step(dt, 26.0f, 0.25f);
  roll.step(dt, 26.0f, 0.25f);
  head.step(dt, 17.0f, 0.20f);
  // The torso goes to the middle of the four feet, a bit behind it (smoothly, and not too far from the body)
  mat3 toBody = transpose(axes);
  vec2 middle(0.0f);
  for (size_t i = 0; i < legs.size(); i++) {
    vec3 tip = toBody * (legs[i].foot - position);
    middle += vec2(tip.x, tip.z) / (float)legs.size();
  }
  middle.y -= params.torsoBias; // (y is z) a bit behind the middle: nearer the hind feet
  float far = length(middle);
  if (far > params.maxTorsoShift)
    middle *= params.maxTorsoShift / far;
  torsoShift += (middle - torsoShift) * std::min(1.0f, dt * params.torsoFollow);
  poseBody();
  aimFace(dt);
}

// All the points of the skeleton, in the body's frame, from the feet and the springs
void SpiderGait::poseBody() {
  const std::vector<int> &spine = rig.spine;
  size_t segments = spine.size() - 1;
  std::vector<vec3> up(segments);
  points[spine[0]] = vec3(0.0f, params.bodyHeight + bob.x, 0.0f);
  for (size_t k = 0; k < segments; k++) {
    float lean = radians(params.lean[std::min(k, params.lean.size() - 1)]) + pitch.x;
    if (k >= 4)
      lean += head.x; // neck and head have a shake of their own
    up[k] = vec3(0.0f, std::cos(lean), std::sin(lean));
    float len = length(bind[spine[k + 1]] - bind[spine[k]]);
    points[spine[k + 1]] = points[spine[k]] + up[k] * len;
  }
  // The middle of the torso (halfway from the pelvis to the chest) is over torsoShift
  {
    const vec3 &a = points[spine[0]], &b = points[spine[std::min((size_t)3, segments)]];
    vec3 offset(torsoShift.x - (a.x + b.x) * 0.5f, 0.0f, torsoShift.y - (a.z + b.z) * 0.5f);
    for (int index : spine)
      points[index] += offset;
  }
  // The shoulders and the hips go with the chest and the pelvis, leaning with the roll
  for (size_t i = 0; i < rig.attach.size(); i++) {
    size_t k = std::min((size_t)rig.attachTo[i], segments - 1);
    vec3 u = up[k];
    vec3 r = vec3(std::cos(roll.x), std::sin(roll.x), 0.0f);
    r = normalize(r - u * dot(r, u));
    vec3 f = cross(r, u);
    vec3 off = bind[rig.attach[i]] - bind[spine[rig.attachTo[i]]];
    points[rig.attach[i]] = points[spine[rig.attachTo[i]]] + r * off.x + u * off.y + f * off.z;
  }
  // The legs: the foot is where it is in the world, and the limb is bent to reach it
  mat3 toBody = transpose(axes);
  for (size_t i = 0; i < legs.size(); i++) {
    const SpiderLeg &leg = rig.legs[i];
    vec3 tip = toBody * (legs[i].foot - position);
    float l1 = length(bind[leg.mid] - bind[leg.root]);
    float l2 = length(bind[leg.end] - bind[leg.mid]);
    float l3 = length(bind[leg.tip] - bind[leg.end]);
    float claw = leg.front ? params.frontClaw : params.hindClaw;
    vec3 pointing = normalize(vec3(leg.side * 0.15f, -claw, 1.0f)); // last bone: forward and down
    vec3 end = tip - pointing * l3;
    vec3 root = points[leg.root];
    vec3 d = end - root;
    float reach = (l1 + l2) * 0.995f;
    if (length(d) > reach) { // too far to reach: the whole limb is pulled in
      end = root + normalize(d) * reach;
      tip = end + pointing * l3;
    }
    vec3 pole = normalize(vec3(leg.side * 0.5f, 1.0f, leg.front ? -0.15f : 0.3f));
    points[leg.mid] = twoBoneMiddle(root, end, l1, l2, pole);
    points[leg.end] = end;
    points[leg.tip] = tip;
  }
}

void SpiderGait::boneGlobals(const vec3 &origin, std::map<std::string, mat4> &out) const {
  rigBoneGlobals(bind, bones, points, origin, out);
  if (!looking)
    return;
  // the look turns the head about its joint and then the neck carries both; that is in the body's
  // frame, and the matrices are relative to `origin`
  mat4 shift = translate(mat4(1.0f), origin), back = translate(mat4(1.0f), -origin);
  auto head = out.find(rig.headBone), neck = out.find(rig.neckBone);
  if (head != out.end())
    head->second = back * neckTurn * headTurn * shift * head->second;
  if (neck != out.end())
    neck->second = back * neckTurn * shift * neck->second;
}

// The face turns to look at the target, like an owl: the head twists about its own axis (the
// neck's), which keeps it upright however far it turns, even looking behind it (the shortest turn
// would tuck it down), and then nods a little to look right at the target. The neck and the head are
// turned about their joints, on top of the pose the points give them.
void SpiderGait::aimFace(float dt) {
  neckTurn = headTurn = mat4(1.0f);
  if (!looking || rig.neckSpine < 0 || rig.headSpine < 0)
    return;
  std::map<std::string, mat4> base;
  rigBoneGlobals(bind, bones, points, vec3(0.0f), base);
  auto head = base.find(rig.headBone);
  if (head == base.end())
    return;
  vec3 neckJoint = points[rig.spine[rig.neckSpine]], headJoint = points[rig.spine[rig.headSpine]];
  vec3 top = points[rig.spine[rig.headSpine + 1]];
  vec3 up = normalize(top - headJoint);                                 // the head's axis
  vec3 face = normalize(mat3(head->second) * vec3(0.0f, 0.0f, 1.0f)); // the face is the model's +z
  vec3 to = transpose(axes) * (lookTarget - position) - (headJoint + top) * 0.5f;
  if (length(to) < 0.05f)
    return;
  to = normalize(to);
  float rate = std::min(1.0f, dt * params.lookRate);

  // The twist: the turn about the head's axis that brings the face to the target's side
  vec3 flatFace = face - up * dot(face, up), flatTo = to - up * dot(to, up);
  if (length(flatFace) > 1e-3f && length(flatTo) > 1e-3f) {
    flatFace = normalize(flatFace);
    flatTo = normalize(flatTo);
    float wanted = std::atan2(dot(up, cross(flatFace, flatTo)), dot(flatFace, flatTo));
    float most = radians(params.maxTwist);
    // the same turn can be made either way round: take the one nearest to where the head is (so
    // that it goes past half a turn instead of spinning back the other way), within its limit
    float best = wanted, bestDistance = 1e9f;
    for (int k = -1; k <= 1; k++) {
      float candidate = wanted + (float)k * 2.0f * 3.14159265f;
      if (std::fabs(candidate) <= most && std::fabs(candidate - twist) < bestDistance) {
        best = candidate;
        bestDistance = std::fabs(candidate - twist);
      }
    }
    if (bestDistance > 1e8f) // (none within the limit: the nearest end)
      best = glm::clamp(wanted, -most, most);
    twist += (best - twist) * rate;
  }
  // The nod: what is left to look right at the target, a little
  vec3 twisted = angleAxis(twist, up) * face;
  quat wantedTilt = rotation(twisted, to);
  float angle = glm::angle(wantedTilt), most = radians(params.maxTilt);
  if (angle > most)
    wantedTilt = slerp(quat(1.0f, 0.0f, 0.0f, 0.0f), wantedTilt, most / angle);
  tilt = slerp(tilt, wantedTilt, rate);

  // The neck does a share of both and the head the rest (the twist is split as angles: as a
  // quaternion, a turn of more than half a turn would be split the wrong way)
  quat none(1.0f, 0.0f, 0.0f, 0.0f);
  float share = params.neckShare;
  quat neckQ = slerp(none, tilt, share) * angleAxis(twist * share, up);
  quat headQ = slerp(none, tilt, 1.0f - share) * angleAxis(twist * (1.0f - share), up);
  neckTurn = translate(mat4(1.0f), neckJoint) * mat4_cast(neckQ) * translate(mat4(1.0f), -neckJoint);
  headTurn = translate(mat4(1.0f), headJoint) * mat4_cast(headQ) * translate(mat4(1.0f), -headJoint);
}
