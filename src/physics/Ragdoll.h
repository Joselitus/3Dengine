#ifndef RAGDOLL
#define RAGDOLL

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "PointRig.h"
#include "ProceduralPose.h"

// A bone of the ragdoll (see RigBone)
typedef RigBone RagdollBone;

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

// A ragdoll of a skinned character (a ProceduralPose): points (joints) with Verlet physics, held together by
// distance links (the bones, braces for the torso, minimum distances so that limbs do not fold
// the wrong way), falling under gravity and held up by the floor and by one more collider (a
// vehicle). Pure physics, no OpenGL. It is told the bind pose (to know the rest lengths and
// directions), started from a pose in the world, stepped, and then gives the world matrix of each
// bone for the skeleton (boneGlobals).
class Ragdoll : public ProceduralPose {
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
  // What it collides with, for the generic step(dt) below
  void setWorld(const FloorQuery &floorQuery, const PushOut &pushOutQuery) {
    floor = floorQuery;
    pushOut = pushOutQuery;
  }
  // ProceduralPose: steps with the world given to setWorld
  void step(double dt) override { step(dt, floor, pushOut); }
  bool isFinished() const override { return asleep; }

  // Holds a point where it is told (e.g. a head held in a mouth) while pinned: it does not fall or
  // give way, and the rest hangs from it. `worldPosition` can change every frame.
  void pin(int point, const glm::vec3 &worldPosition) {
    pinned = point;
    pinPosition = worldPosition;
    asleep = false;
    quietTime = 0.0f;
  }
  void unpin() { pinned = -1; }
  // How thick each point is (its sphere's radius, for the floor and the vehicle) when they differ: a
  // body's spine is much thicker than its fingers. Without it, all use `radius`.
  void setRadii(const std::vector<float> &perPoint) { radii = perPoint; }
  bool isPinned() const { return pinned >= 0; }

  // The bones as line segments (their two points), for drawing the skeleton
  void segments(std::vector<std::pair<glm::vec3, glm::vec3>> &out) const {
    for (const RagdollBone &b : bones)
      out.push_back(std::make_pair(points[b.from], points[b.to]));
  }
  bool isAsleep() const { return asleep; }
  const std::vector<glm::vec3> &getPoints() const { return points; }
  // The world matrix of every bone (by name), relative to `origin`: rotation and the position of
  // its first point
  void boneGlobals(const glm::vec3 &origin,
                   std::map<std::string, glm::mat4> &out) const override;

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
  FloorQuery floor;
  PushOut pushOut;
  bool asleep = false;
  float quietTime = 0.0f;
  std::vector<float> radii;
  int pinned = -1;
  glm::vec3 pinPosition = glm::vec3(0.0f);
};

#endif
