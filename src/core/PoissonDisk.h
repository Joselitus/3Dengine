#ifndef POISSON_DISK
#define POISSON_DISK

#include <cstdint>
#include <functional>
#include <vector>

#include <glm/glm.hpp>

// Scatters points over a rectangle so that no two are closer than `minDistance` and the gaps
// between them are as even as chance allows (a Poisson-disk distribution, Bridson's algorithm:
// each new point is tried in the ring between 1 and 2 times the distance round a point already
// placed, up to `tries` times, with a grid of cells to find the neighbours fast). It is how forest
// tools such as SpeedTree's populate a forest with trees that keep a minimum spacing, without a
// visible pattern. `allowed` (optional) can reject places (a clearing, a road). Fixed seed: the
// same points every time. No OpenGL.
std::vector<glm::vec2> poissonDisk(const glm::vec2 &min, const glm::vec2 &max, float minDistance,
                                   uint32_t seed, int tries = 30,
                                   std::function<bool(const glm::vec2 &)> allowed = nullptr);

#endif
