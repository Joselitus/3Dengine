#include "PointRig.h"

#include <cmath>

using namespace glm;

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

void rigBoneGlobals(const std::vector<vec3> &bind, const std::vector<RigBone> &bones,
                    const std::vector<vec3> &points, const vec3 &origin,
                    std::map<std::string, mat4> &out) {
  for (const RigBone &b : bones) {
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
