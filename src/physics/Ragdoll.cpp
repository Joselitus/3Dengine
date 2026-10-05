#include "Ragdoll.h"

#include <algorithm>
#include <cmath>

using namespace glm;

#define SUBSTEPS 4
#define ITERATIONS 8
// It is put to sleep (not simulated any more) after being this still for this long
#define SLEEP_SPEED 0.06f
#define SLEEP_TIME 1.2f

Ragdoll::Ragdoll(const std::vector<vec3> &bindPoints, const std::vector<RagdollBone> &bones,
                 const std::vector<RagdollLink> &ragdollLinks)
    : bind(bindPoints), bones(bones) {
  for (const RagdollLink &l : ragdollLinks) {
    Link link;
    static_cast<RagdollLink &>(link) = l;
    link.rest = length(bind[l.a] - bind[l.b]);
    links.push_back(link);
  }
  // The bones are rigid links between their two points too
  for (const RagdollBone &b : bones) {
    Link link;
    link.a = b.from;
    link.b = b.to;
    link.rest = length(bind[b.from] - bind[b.to]);
    links.push_back(link);
  }
  points = previous = bind;
}

void Ragdoll::start(const std::vector<vec3> &worldPoints, const vec3 &velocity) {
  points = worldPoints;
  previous.resize(points.size());
  // (Verlet keeps its velocity as the distance moved in one sub-step)
  for (size_t i = 0; i < points.size(); i++)
    previous[i] = points[i] - velocity * (1.0f / 60.0f / SUBSTEPS);
  asleep = false;
  quietTime = 0.0f;
}

void Ragdoll::step(double dt, const FloorQuery &floor, const PushOut &pushOut) {
  if (asleep)
    return;
  float h = (float)dt / SUBSTEPS;
  for (int s = 0; s < SUBSTEPS; s++) {
    // Verlet: carry on as before, plus gravity
    for (size_t i = 0; i < points.size(); i++) {
      vec3 velocity = (points[i] - previous[i]) * damping;
      previous[i] = points[i];
      points[i] += velocity + vec3(0.0f, -gravity * h * h, 0.0f);
    }
    for (int it = 0; it < ITERATIONS; it++) {
      for (const Link &l : links) {
        vec3 d = points[l.b] - points[l.a];
        float len = length(d);
        if (len < 1e-6f)
          continue;
        float target = l.rest;
        if (l.minOnly) {
          float lowest = l.rest * l.minRatio;
          if (len >= lowest)
            continue;
          target = lowest;
        }
        vec3 correction = d / len * ((len - target) * 0.5f * l.stiffness);
        points[l.a] += correction;
        points[l.b] -= correction;
      }
      for (vec3 &p : points) {
        if (pushOut)
          pushOut(p, radius);
        float ground;
        if (floor && floor(p.x, p.z, ground) && p.y < ground + radius) {
          p.y = ground + radius;
          // on the floor: it loses most of what it was sliding at
          vec3 &before = previous[&p - &points[0]];
          before.x = p.x - (p.x - before.x) * groundFriction;
          before.z = p.z - (p.z - before.z) * groundFriction;
          before.y = p.y;
        }
      }
    }
  }
  // Asleep once nothing moves
  float fastest = 0.0f;
  for (size_t i = 0; i < points.size(); i++)
    fastest = std::max(fastest, length(points[i] - previous[i]) / h);
  quietTime = fastest < SLEEP_SPEED ? quietTime + (float)dt : 0.0f;
  if (quietTime > SLEEP_TIME)
    asleep = true;
}

// A frame (columns: along, up, side) from a direction and a roll hint
static mat3 frameOf(const vec3 &along, const vec3 &hint) {
  vec3 a = normalize(along);
  vec3 u = hint - a * dot(hint, a);
  if (length(u) < 1e-5f)
    u = abs(a.y) < 0.9f ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f);
  u = normalize(u - a * dot(u, a));
  return mat3(a, u, cross(a, u));
}

// The shortest turn from direction a to direction b
static mat3 arc(const vec3 &a, const vec3 &b) {
  vec3 from = normalize(a), to = normalize(b);
  float c = dot(from, to);
  if (c > 0.99999f)
    return mat3(1.0f);
  vec3 axis;
  if (c < -0.99999f) { // opposite: half a turn about anything perpendicular
    axis = abs(from.x) < 0.9f ? cross(from, vec3(1, 0, 0)) : cross(from, vec3(0, 1, 0));
    axis = normalize(axis);
    return mat3(2.0f * outerProduct(axis, axis) - mat3(1.0f));
  }
  axis = cross(from, to);
  float s = length(axis);
  axis /= s;
  float angle = std::atan2(s, c);
  float cs = std::cos(angle), sn = std::sin(angle);
  mat3 K(vec3(0, axis.z, -axis.y), vec3(-axis.z, 0, axis.x), vec3(axis.y, -axis.x, 0));
  return mat3(1.0f) + sn * K + (1.0f - cs) * (K * K);
}

void Ragdoll::boneGlobals(const vec3 &origin, std::map<std::string, mat4> &out) const {
  for (const RagdollBone &b : bones) {
    vec3 d0 = bind[b.to] - bind[b.from], d1 = points[b.to] - points[b.from];
    mat3 R;
    if (b.sideA >= 0 && b.sideB >= 0) {
      vec3 s0 = bind[b.sideA] - bind[b.sideB], s1 = points[b.sideA] - points[b.sideB];
      R = frameOf(d1, s1) * transpose(frameOf(d0, s0));
    } else {
      R = arc(d0, d1);
    }
    mat4 m(R);
    m[3] = vec4(points[b.from] - origin, 1.0f);
    out[b.name] = m;
  }
}
