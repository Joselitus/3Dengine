#include "CollisionShape.h"

#include <algorithm>
#include <cmath>

using namespace glm;

// --------------------------------------------------------------- helpers
namespace {

// Closest points between the segments p1-q1 and p2-q2 (Ericson, Real-Time
// Collision Detection 5.1.9)
void closestSegments(const vec3 &p1, const vec3 &q1, const vec3 &p2,
                     const vec3 &q2, vec3 &c1, vec3 &c2) {
  vec3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
  float a = dot(d1, d1), e = dot(d2, d2), f = dot(d2, r);
  float s, t;
  const float eps = 1e-8f;
  if (a <= eps && e <= eps) {
    s = t = 0.0f;
  } else if (a <= eps) {
    s = 0.0f;
    t = clamp(f / e, 0.0f, 1.0f);
  } else {
    float c = dot(d1, r);
    if (e <= eps) {
      t = 0.0f;
      s = clamp(-c / a, 0.0f, 1.0f);
    } else {
      float b = dot(d1, d2), denom = a * e - b * b;
      s = denom > eps ? clamp((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
      t = (b * s + f) / e;
      if (t < 0.0f) {
        t = 0.0f;
        s = clamp(-c / a, 0.0f, 1.0f);
      } else if (t > 1.0f) {
        t = 1.0f;
        s = clamp((b - c) / a, 0.0f, 1.0f);
      }
    }
  }
  c1 = p1 + d1 * s;
  c2 = p2 + d2 * t;
}

struct OrientedBox {
  vec3 centre, half;
  vec3 axis[3];
};

OrientedBox orient(const Box &box, const Pose &pose) {
  OrientedBox b;
  box.world(pose, b.centre, b.half);
  for (int i = 0; i < 3; i++)
    b.axis[i] = pose.rotation[i];
  return b;
}

bool capsuleCapsule(const Capsule &ca, const Pose &pa, const Capsule &cb,
                    const Pose &pb, Contact &out) {
  vec3 a0, a1, b0, b1;
  float ra, rb;
  ca.segment(pa, a0, a1, ra);
  cb.segment(pb, b0, b1, rb);
  vec3 pa_, pb_;
  closestSegments(a0, a1, b0, b1, pa_, pb_);
  vec3 d = pb_ - pa_;
  float dist = length(d);
  float depth = ra + rb - dist;
  if (depth <= 0.0f)
    return false;
  if (dist > 1e-5f) {
    out.normal = d / dist;
  } else {
    // The axes cross: push apart sideways
    vec3 side = (b0 + b1 - a0 - a1) * 0.5f;
    side.y = 0.0f;
    out.normal = length(side) > 1e-5f ? normalize(side) : vec3(1, 0, 0);
  }
  out.depth = depth;
  return true;
}

// The capsule is A
bool capsuleBox(const Capsule &cap, const Pose &pc, const Box &box,
                const Pose &pbox, Contact &out) {
  vec3 a, b;
  float r;
  cap.segment(pc, a, b, r);
  OrientedBox ob = orient(box, pbox);
  mat3 axes(ob.axis[0], ob.axis[1], ob.axis[2]);
  mat3 toLocal = transpose(axes);
  vec3 la = toLocal * (a - ob.centre), lb = toLocal * (b - ob.centre);

  // The distance from the segment to the (convex) box is convex along the
  // segment: find its lowest point
  auto distance2 = [&](float t) {
    vec3 p = mix(la, lb, t);
    vec3 d = p - clamp(p, -ob.half, ob.half);
    return dot(d, d);
  };
  float lo = 0.0f, hi = 1.0f;
  for (int i = 0; i < 30; i++) {
    float m1 = lo + (hi - lo) / 3.0f, m2 = hi - (hi - lo) / 3.0f;
    if (distance2(m1) < distance2(m2))
      hi = m2;
    else
      lo = m1;
  }
  vec3 p = mix(la, lb, (lo + hi) * 0.5f);
  vec3 q = clamp(p, -ob.half, ob.half);
  vec3 d = q - p; // from the capsule to the box
  float dist = length(d);
  if (dist > 1e-5f) {
    if (dist >= r)
      return false;
    out.normal = axes * (d / dist);
    out.depth = r - dist;
    return true;
  }
  // The core touches the box: leave through the face that needs the least
  // push to clear the whole segment (and the radius)
  int bestAxis = 0;
  float bestPush = 1e30f, bestSign = 1.0f;
  for (int i = 0; i < 3; i++) {
    // out through +face i: the lowest point of the segment must pass h + r
    float up = ob.half[i] + r - std::min(la[i], lb[i]);
    // out through -face i
    float down = ob.half[i] + r + std::max(la[i], lb[i]);
    if (up < bestPush) {
      bestPush = up;
      bestAxis = i;
      bestSign = 1.0f;
    }
    if (down < bestPush) {
      bestPush = down;
      bestAxis = i;
      bestSign = -1.0f;
    }
  }
  // the capsule moves along bestSign * axis, so the box is the other way
  out.normal = -bestSign * ob.axis[bestAxis];
  out.depth = bestPush;
  return true;
}

bool boxBox(const Box &ba, const Pose &pa, const Box &bb, const Pose &pb,
            Contact &out) {
  OrientedBox A = orient(ba, pa), B = orient(bb, pb);
  vec3 t = B.centre - A.centre;
  float bestScore = 1e30f, bestDepth = 0.0f;
  vec3 bestNormal(0, 1, 0);

  // Separating axis test; the smallest overlap is the way out
  auto test = [&](vec3 axis, bool edge) {
    float len = length(axis);
    if (len < 1e-4f)
      return true; // parallel edges: the face axes cover it
    axis /= len;
    float ra = 0.0f, rb = 0.0f;
    for (int i = 0; i < 3; i++) {
      ra += A.half[i] * fabs(dot(A.axis[i], axis));
      rb += B.half[i] * fabs(dot(B.axis[i], axis));
    }
    float d = dot(t, axis);
    float overlap = ra + rb - fabs(d);
    if (overlap < 0.0f)
      return false;
    // Prefer the faces over the edges when they are close (steadier)
    float score = edge ? overlap * 1.05f + 0.01f : overlap;
    if (score < bestScore) {
      bestScore = score;
      bestDepth = overlap;
      bestNormal = d >= 0.0f ? axis : -axis;
    }
    return true;
  };
  for (int i = 0; i < 3; i++)
    if (!test(A.axis[i], false) || !test(B.axis[i], false))
      return false;
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      if (!test(cross(A.axis[i], B.axis[j]), true))
        return false;
  out.normal = bestNormal;
  out.depth = bestDepth;
  return bestDepth > 0.0f;
}

} // namespace

bool CollisionShape::collide(const CollisionShape &a, const Pose &pa,
                             const CollisionShape &b, const Pose &pb,
                             Contact &contact) {
  if (a.type() == CAPSULE && b.type() == CAPSULE)
    return capsuleCapsule(static_cast<const Capsule &>(a), pa,
                          static_cast<const Capsule &>(b), pb, contact);
  if (a.type() == CAPSULE)
    return capsuleBox(static_cast<const Capsule &>(a), pa,
                      static_cast<const Box &>(b), pb, contact);
  if (b.type() == CAPSULE) {
    if (!capsuleBox(static_cast<const Capsule &>(b), pb,
                    static_cast<const Box &>(a), pa, contact))
      return false;
    contact.normal = -contact.normal; // it was computed from b to a
    return true;
  }
  return boxBox(static_cast<const Box &>(a), pa, static_cast<const Box &>(b),
                pb, contact);
}

// --------------------------------------------------------------- Capsule
Capsule::Capsule(float radius, float height, const vec3 &base)
    : radius(radius), height(height), base(base) {
  // Never thinner than a sphere
  this->radius = std::min(radius, height * 0.5f);
}

std::shared_ptr<Capsule> Capsule::fit(const vec3 &min, const vec3 &max) {
  vec3 size = max - min;
  float radius = 0.5f * std::max(size.x, size.z);
  return std::make_shared<Capsule>(radius, size.y,
                                   vec3((min.x + max.x) * 0.5f, min.y,
                                        (min.z + max.z) * 0.5f));
}

void Capsule::segment(const Pose &pose, vec3 &a, vec3 &b, float &r) const {
  vec3 low = base + vec3(0.0f, radius, 0.0f);
  vec3 high = base + vec3(0.0f, height - radius, 0.0f);
  a = pose.position + pose.rotation * (pose.scale * low);
  b = pose.position + pose.rotation * (pose.scale * high);
  r = radius * pose.scale;
}

void Capsule::bounds(const Pose &pose, vec3 &min, vec3 &max) const {
  vec3 a, b;
  float r;
  segment(pose, a, b, r);
  min = glm::min(a, b) - vec3(r);
  max = glm::max(a, b) + vec3(r);
}

void Capsule::floorSamples(const Pose &pose, std::vector<vec3> &out) const {
  vec3 a, b;
  float r;
  segment(pose, a, b, r);
  vec3 lowest = a.y < b.y ? a : b;
  out.push_back(lowest - vec3(0.0f, r, 0.0f));
}

// ------------------------------------------------------------------- Box
void Box::world(const Pose &pose, vec3 &c, vec3 &h) const {
  c = pose.position + pose.rotation * (pose.scale * center);
  h = halfExtents * pose.scale;
}

void Box::bounds(const Pose &pose, vec3 &min, vec3 &max) const {
  vec3 c, h;
  world(pose, c, h);
  vec3 extent(0.0f);
  for (int i = 0; i < 3; i++)
    extent += abs(pose.rotation[i]) * h[i];
  min = c - extent;
  max = c + extent;
}

void Box::floorSamples(const Pose &pose, std::vector<vec3> &out) const {
  vec3 c, h;
  world(pose, c, h);
  // The eight corners, and a grid (about a metre apart) on the face that
  // points most downwards: whichever way up the box is, its lowest part is
  // covered. The grid keeps a long box from touching the floor between
  // its corners.
  for (int i = 0; i < 8; i++) {
    vec3 corner = c;
    for (int k = 0; k < 3; k++)
      corner += pose.rotation[k] * (h[k] * ((i >> k) & 1 ? 1.0f : -1.0f));
    out.push_back(corner);
  }
  int down = 0;
  float lowest = 0.0f, sign = -1.0f;
  for (int k = 0; k < 3; k++) {
    float y = pose.rotation[k].y; // how much axis k points up
    if (fabs(y) > fabs(lowest)) {
      lowest = y;
      down = k;
      sign = y > 0.0f ? -1.0f : 1.0f; // the face on the side that goes down
    }
  }
  int u = (down + 1) % 3, v = (down + 2) % 3;
  int nu = std::max(2, (int)ceil(2.0f * h[u] / 1.0f) + 1);
  int nv = std::max(2, (int)ceil(2.0f * h[v] / 1.0f) + 1);
  for (int i = 0; i < nu; i++)
    for (int j = 0; j < nv; j++)
      out.push_back(c + pose.rotation[down] * (sign * h[down]) +
                    pose.rotation[u] * (h[u] * (2.0f * i / (nu - 1) - 1.0f)) +
                    pose.rotation[v] * (h[v] * (2.0f * j / (nv - 1) - 1.0f)));
}

// --------------------------------------------------------------- raycasts
namespace {

// Ray (unit direction) against a sphere: distance to the first hit, 0 if the
// ray starts inside
bool raySphere(const vec3 &origin, const vec3 &direction, const vec3 &centre,
               float radius, float &distance) {
  vec3 m = origin - centre;
  float b = dot(m, direction), c = dot(m, m) - radius * radius;
  if (c > 0.0f && b > 0.0f)
    return false; // outside and moving away
  float discriminant = b * b - c;
  if (discriminant < 0.0f)
    return false;
  distance = std::max(0.0f, -b - std::sqrt(discriminant));
  return true;
}

} // namespace

bool Capsule::raycast(const Pose &pose, const vec3 &origin,
                      const vec3 &direction, float &distance) const {
  vec3 a, b;
  float r;
  segment(pose, a, b, r);
  bool hit = false;
  float t;
  // The two end spheres
  if (raySphere(origin, direction, a, r, t)) {
    distance = t;
    hit = true;
  }
  if (raySphere(origin, direction, b, r, t) && (!hit || t < distance)) {
    distance = t;
    hit = true;
  }
  // The cylinder between them: the ray and the axis, without their parts
  // along the axis, must be r apart
  vec3 axis = b - a;
  float length = glm::length(axis);
  if (length < 1e-6f)
    return hit;
  axis /= length;
  vec3 m = origin - a;
  vec3 d = direction - axis * dot(direction, axis);
  vec3 mp = m - axis * dot(m, axis);
  float qa = dot(d, d), qb = dot(mp, d), qc = dot(mp, mp) - r * r;
  if (qa < 1e-10f)
    return hit; // parallel to the axis: only the spheres can be hit first
  float discriminant = qb * qb - qa * qc;
  if (discriminant < 0.0f)
    return hit;
  t = std::max(0.0f, (-qb - std::sqrt(discriminant)) / qa);
  float along = dot(m + direction * t, axis);
  if (along >= 0.0f && along <= length && (qc <= 0.0f || -qb >= 0.0f) &&
      (!hit || t < distance)) {
    distance = t;
    hit = true;
  }
  return hit;
}

bool Box::raycast(const Pose &pose, const vec3 &origin, const vec3 &direction,
                  float &distance) const {
  // Slabs, in the box's own axes
  vec3 c, h;
  world(pose, c, h);
  vec3 o = origin - c;
  float near = 0.0f, far = 1e30f;
  for (int k = 0; k < 3; k++) {
    float start = dot(o, pose.rotation[k]);
    float speed = dot(direction, pose.rotation[k]);
    if (std::fabs(speed) < 1e-8f) {
      if (start < -h[k] || start > h[k])
        return false; // parallel to this slab and outside it
      continue;
    }
    float t0 = (-h[k] - start) / speed, t1 = (h[k] - start) / speed;
    if (t0 > t1)
      std::swap(t0, t1);
    near = std::max(near, t0);
    far = std::min(far, t1);
    if (near > far)
      return false;
  }
  distance = near;
  return true;
}
