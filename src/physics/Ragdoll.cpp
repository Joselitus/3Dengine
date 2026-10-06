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
      if (pinned >= 0) {
        points[pinned] = pinPosition;
        previous[pinned] = pinPosition;
      }
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
        vec3 correction = d / len * ((len - target) * l.stiffness);
        // (a pinned point does not move: the other takes all of it)
        float wa = (int)l.a == pinned ? 0.0f : 1.0f, wb = (int)l.b == pinned ? 0.0f : 1.0f;
        if (wa + wb == 0.0f)
          continue;
        points[l.a] += correction * (wa / (wa + wb));
        points[l.b] -= correction * (wb / (wa + wb));
      }
      for (size_t i = 0; i < points.size(); i++) {
        vec3 &p = points[i];
        float r = i < radii.size() ? radii[i] : radius;
        if (pushOut)
          pushOut(p, r);
        float ground;
        if (floor && floor(p.x, p.z, ground) && p.y < ground + r) {
          p.y = ground + r;
          // on the floor: it loses most of what it was sliding at
          vec3 &before = previous[i];
          before.x = p.x - (p.x - before.x) * groundFriction;
          before.z = p.z - (p.z - before.z) * groundFriction;
          before.y = p.y;
        }
      }
    }
  }
  if (pinned >= 0) // (held: it never sleeps)
    return;
  // Asleep once nothing moves
  float fastest = 0.0f;
  for (size_t i = 0; i < points.size(); i++)
    fastest = std::max(fastest, length(points[i] - previous[i]) / h);
  quietTime = fastest < SLEEP_SPEED ? quietTime + (float)dt : 0.0f;
  if (quietTime > SLEEP_TIME)
    asleep = true;
}

void Ragdoll::boneGlobals(const vec3 &origin, std::map<std::string, mat4> &out) const {
  rigBoneGlobals(bind, bones, points, origin, out);
}
