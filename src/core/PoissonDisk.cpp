#include "PoissonDisk.h"

#include <cmath>
#include <random>

using namespace glm;

std::vector<vec2> poissonDisk(const vec2 &min, const vec2 &max, float r, uint32_t seed, int tries,
                              std::function<bool(const vec2 &)> allowed) {
  std::vector<vec2> points;
  if (r <= 0.0f || max.x <= min.x || max.y <= min.y)
    return points;
  std::mt19937 random(seed);
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);
  // a grid of cells r / sqrt(2) wide: each cell holds at most one point
  const float cell = r / std::sqrt(2.0f);
  const int cols = (int)std::ceil((max.x - min.x) / cell), rows = (int)std::ceil((max.y - min.y) / cell);
  std::vector<int> grid(cols * rows, -1);
  auto cellOf = [&](const vec2 &p, int &cx, int &cy) {
    cx = std::min(cols - 1, (int)((p.x - min.x) / cell));
    cy = std::min(rows - 1, (int)((p.y - min.y) / cell));
  };
  auto fits = [&](const vec2 &p) {
    if (p.x < min.x || p.y < min.y || p.x >= max.x || p.y >= max.y)
      return false;
    if (allowed && !allowed(p))
      return false;
    int cx, cy;
    cellOf(p, cx, cy);
    for (int y = std::max(0, cy - 2); y <= std::min(rows - 1, cy + 2); y++)
      for (int x = std::max(0, cx - 2); x <= std::min(cols - 1, cx + 2); x++) {
        int i = grid[y * cols + x];
        if (i >= 0 && distance(points[i], p) < r)
          return false;
      }
    return true;
  };
  auto add = [&](const vec2 &p) {
    int cx, cy;
    cellOf(p, cx, cy);
    grid[cy * cols + cx] = (int)points.size();
    points.push_back(p);
  };

  // the first point: anywhere allowed
  std::vector<int> active;
  for (int attempt = 0; attempt < 1000 && points.empty(); attempt++) {
    vec2 p(min.x + unit(random) * (max.x - min.x), min.y + unit(random) * (max.y - min.y));
    if (fits(p)) {
      add(p);
      active.push_back(0);
    }
  }
  // grow from the active points until none can take another round it
  while (!active.empty()) {
    size_t pick = (size_t)(unit(random) * active.size()) % active.size();
    vec2 centre = points[active[pick]];
    bool placed = false;
    for (int k = 0; k < tries; k++) {
      float a = unit(random) * 6.2831853f;
      float d = r * std::sqrt(1.0f + 3.0f * unit(random)); // uniform over the ring's area
      vec2 p = centre + vec2(std::cos(a), std::sin(a)) * d;
      if (fits(p)) {
        active.push_back((int)points.size());
        add(p);
        placed = true;
        break;
      }
    }
    if (!placed) {
      active[pick] = active.back();
      active.pop_back();
    }
  }
  return points;
}
