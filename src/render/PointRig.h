#ifndef POINTRIG
#define POINTRIG

#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

// A bone of a skeleton posed from points: it goes from one point to another. `sideA`/`sideB`
// (or -1) are two points whose difference gives its roll about the bone (e.g. the shoulders for
// the chest), so that it does not twist; without them the bone just takes the shortest turn.
struct RigBone {
  std::string name;
  int from, to;
  int sideA, sideB;
  RigBone(const std::string &name, int from, int to, int sideA = -1, int sideB = -1)
      : name(name), from(from), to(to), sideA(sideA), sideB(sideB) {}
};

// The world matrix of every bone (by name, relative to `origin`) of a skeleton whose joints
// are at `points` now and were at `bind` in the pose of the model: the rotation that takes each
// bone from its bind direction to its direction now, and the position of its first point. This
// is what AnimatedModel::setBoneGlobals takes. Shared by the procedural poses that move points
// (Ragdoll, SpiderGait). No OpenGL.
void rigBoneGlobals(const std::vector<glm::vec3> &bind, const std::vector<RigBone> &bones,
                    const std::vector<glm::vec3> &points, const glm::vec3 &origin,
                    std::map<std::string, glm::mat4> &out);

#endif
