#include "RoadSet.h"

#include <algorithm>
#include <cmath>

using namespace std;
using namespace glm;

namespace {
// A point of the centripetal Catmull-Rom spline between p1 and p2 (Barry-Goldman), t in [0, 1]
vec2 splinePoint(const vec2 &p0, const vec2 &p1, const vec2 &p2, const vec2 &p3, float t) {
  auto knot = [](const vec2 &a, const vec2 &b) { return std::max(std::sqrt(length(b - a)), 1e-3f); };
  float t0 = 0.0f, t1 = t0 + knot(p0, p1), t2 = t1 + knot(p1, p2), t3 = t2 + knot(p2, p3);
  float u = t1 + (t2 - t1) * t;
  vec2 a1 = (t1 - u) / (t1 - t0) * p0 + (u - t0) / (t1 - t0) * p1;
  vec2 a2 = (t2 - u) / (t2 - t1) * p1 + (u - t1) / (t2 - t1) * p2;
  vec2 a3 = (t3 - u) / (t3 - t2) * p2 + (u - t2) / (t3 - t2) * p3;
  vec2 b1 = (t2 - u) / (t2 - t0) * a1 + (u - t0) / (t2 - t0) * a2;
  vec2 b2 = (t3 - u) / (t3 - t1) * a2 + (u - t1) / (t3 - t1) * a3;
  return (t2 - u) / (t2 - t1) * b1 + (u - t1) / (t2 - t1) * b2;
}
} // namespace

vector<RoadSample> sampleRoad(const Road &road, float step) {
  vector<RoadSample> out;
  int n = (int)road.points.size();
  if (n < 2 || step <= 0.0f)
    return out;
  // The spline, finely: each piece between two control points in as many bits as its length asks
  vector<vec2> dense;
  int pieces = road.closed ? n : n - 1;
  auto at = [&](int i) {
    if (road.closed)
      return road.points[((i % n) + n) % n];
    return road.points[std::min(std::max(i, 0), n - 1)];
  };
  for (int i = 0; i < pieces; i++) {
    vec2 p0 = at(i - 1), p1 = at(i), p2 = at(i + 1), p3 = at(i + 2);
    // (an open road's ends: mirror the neighbour, so that it leaves straight)
    if (!road.closed && i == 0)
      p0 = 2.0f * p1 - p2;
    if (!road.closed && i == pieces - 1)
      p3 = 2.0f * p2 - p1;
    int bits = std::max(4, (int)std::ceil(length(p2 - p1) * 2.0f));
    for (int k = 0; k < bits; k++)
      dense.push_back(splinePoint(p0, p1, p2, p3, (float)k / bits));
  }
  dense.push_back(road.closed ? road.points[0] : road.points.back());
  // Even steps along it
  vector<float> along(dense.size(), 0.0f);
  for (size_t i = 1; i < dense.size(); i++)
    along[i] = along[i - 1] + length(dense[i] - dense[i - 1]);
  float total = along.back();
  if (total < 1e-3f)
    return out;
  int count = std::max(2, (int)std::ceil(total / step) + 1);
  size_t j = 0;
  for (int i = 0; i < count; i++) {
    float d = total * i / (count - 1);
    while (j + 2 < dense.size() && along[j + 1] < d)
      j++;
    float span = std::max(along[j + 1] - along[j], 1e-6f);
    float f = clamp((d - along[j]) / span, 0.0f, 1.0f);
    RoadSample s;
    s.position = mix(dense[j], dense[j + 1], f);
    s.distance = d;
    s.tangent = vec2(0.0f, 1.0f);
    out.push_back(s);
  }
  for (int i = 0; i < count; i++) {
    int a = std::max(i - 1, 0), b = std::min(i + 1, count - 1);
    vec2 t = out[b].position - out[a].position;
    if (road.closed && (i == 0 || i == count - 1)) // (the same point: the direction across the join)
      t = out[1].position - out[count - 2].position;
    if (length(t) > 1e-6f)
      out[i].tangent = normalize(t);
    else if (i > 0)
      out[i].tangent = out[i - 1].tangent;
  }
  return out;
}

void RoadSet::set(const vector<Road> &roads) {
  segments.clear();
  grid.clear();
  for (const Road &road : roads) {
    vector<RoadSample> samples = sampleRoad(road, 1.0f);
    for (size_t i = 0; i + 1 < samples.size(); i++) {
      Segment s = {samples[i].position, samples[i + 1].position, road.width * 0.5f, road.type};
      unsigned int id = (unsigned int)segments.size();
      segments.push_back(s);
      vec2 lo = min(s.a, s.b) - vec2(s.halfWidth), hi = max(s.a, s.b) + vec2(s.halfWidth);
      for (int cz = (int)std::floor(lo.y / CELL); cz <= (int)std::floor(hi.y / CELL); cz++)
        for (int cx = (int)std::floor(lo.x / CELL); cx <= (int)std::floor(hi.x / CELL); cx++)
          grid[((long long)cz << 32) ^ (unsigned int)cx].push_back(id);
    }
  }
}

bool RoadSet::surfaceAt(float x, float z, FloorMaterial &material) const {
  if (segments.empty())
    return false;
  auto cell = grid.find(((long long)(int)std::floor(z / CELL) << 32) ^ (unsigned int)(int)std::floor(x / CELL));
  if (cell == grid.end())
    return false;
  vec2 p(x, z);
  int best = -1;
  for (unsigned int id : cell->second) {
    const Segment &s = segments[id];
    vec2 ab = s.b - s.a;
    float t = clamp(dot(p - s.a, ab) / std::max(dot(ab, ab), 1e-6f), 0.0f, 1.0f);
    if (length(p - (s.a + ab * t)) <= s.halfWidth && (int)id > best)
      best = (int)id;
  }
  if (best < 0)
    return false;
  material = segments[best].type == Road::Dirt ? FloorMaterial::Dirt : FloorMaterial::Asphalt;
  return true;
}
