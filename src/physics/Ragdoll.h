#ifndef RAGDOLL
#define RAGDOLL

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

// A bone of the skeleton: it goes from one point of the ragdoll to another. `sideA`/`sideB`
// (or -1) are two points whose difference gives its roll about the bone (e.g. the shoulders for
// the chest), so that it does not twist; without them the bone just takes the shortest turn.
struct RagdollBone {
  std::string name;
  int from, to;
  int sideA, sideB;
  RagdollBone(const std::string &name, int from, int to, int sideA = -1, int sideB = -1)
      : name(name), from(from), to(to), sideA(sideA), sideB(sideB) {}
};

// Two points kept at a distance. `stiffness` 1 = rigid. With `minOnly` the distance may be
// larger than the rest one but not shorter than rest * minRatio (a joint that can't fold back).
struct RagdollLink {
  int a, b;
  float stiffness;
  bool minOnly;
  float minRatio;
  RagdollLink() : a(0), b(0), stiffness(1.0f), minOnly(false), minRatio(1.0f) {}
  RagdollLink(int a, int b, float stiffness = 1.0f, bool minOnly = false, float minRatio = 1.0f)
      : a(a), b(b), stiffness(stiffness), minOnly(minOnly), minRatio(minRatio) {}
};

// A ragdoll of a skinned character: points (joints) with Verlet physics, held together by
// distance links (the bones, braces for the torso, minimum distances so that limbs do not fold
// the wrong way), falling under gravity and held up by the floor and by one more collider (a
// vehicle). Pure physics, no OpenGL. It is told the bind pose (to know the rest lengths and
// directions), started from a pose in the world, stepped, and then gives the world matrix of each
// bone for the skeleton (boneGlobals).
class Ragdoll {
public:
  // The floor under (x, z): its height, false if there is none there
  typedef std::function<bool(float x, float z, float &height)> FloorQuery;
  // Pushes a sphere of this radius out of the vehicle or whatever else it is inside
  typedef std::function<void(glm::vec3 &point, float radius)> PushOut;

  Ragdoll(const std::vector<glm::vec3> &bindPoints, const std::vector<RagdollBone> &bones,
          const std::vector<RagdollLink> &links);

  // Starts it: the points at these world positions and all moving at `velocity`
  void start(const std::vector<glm::vec3> &worldPoints, const glm::vec3 &velocity);
  void step(double dt, const FloorQuery &floor, const PushOut &pushOut);

  bool isAsleep() const { return asleep; }
  const std::vector<glm::vec3> &getPoints() const { return points; }
  // The world matrix of every bone (by name), relative to `origin`: rotation and the position of
  // its first point
  void boneGlobals(const glm::vec3 &origin, std::map<std::string, glm::mat4> &out) const;

  // How it moves
  float gravity = 20.0f;      // m/s^2
  float damping = 0.995f;     // of the velocity, per sub-step
  float radius = 0.07f;       // of every point, for the floor and the vehicle
  float groundFriction = 0.82f; // horizontal velocity kept per sub-step while on the floor

private:
  std::vector<glm::vec3> bind;
  std::vector<RagdollBone> bones;
  struct Link : RagdollLink {
    float rest;
  };
  std::vector<Link> links;
  std::vector<glm::vec3> points, previous;
  bool asleep = false;
  float quietTime = 0.0f;
};

#endif
