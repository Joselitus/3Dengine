#ifndef INSECT_LEGS
#define INSECT_LEGS

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "PointRig.h"
#include "ProceduralPose.h"

// How the legs of InsectLegs move (a top-level struct: with its initialisers it could not be a
// default argument inside the class)
struct InsectLegParams {
  float stiffness[4] = {160.0f, 110.0f, 70.0f, 45.0f}; // the spring of joints 1..4 (1/s^2):
                                                       // softer towards the foot (it whips)
  float damping = 5.0f;    // 1/s
  float gravity = 4.0f;    // m/s^2: the muscles hold most of it
  float subStep = 1.0f / 240.0f;
  int iterations = 3;      // of the length constraints
  float footClearance = 0.03f; // no joint goes below the floor plus this
};

// The legs of a flying body (an insect) as a ProceduralPose: each leg is a chain of joints
// (hip ... foot) whose hip is fixed to the body and whose other joints have mass (Verlet). Each
// joint is pulled by a spring towards where the pose wants it (setTargets, in the body's frame:
// hanging, grasping, curled up...), the segments keep their lengths and gravity pulls a little,
// so when the body accelerates, turns or stops the legs lag behind, swing and settle: they carry
// inertia. Stepped in small fixed sub-steps, so it is stable at any frame rate. No OpenGL.
//
// Each frame: setBody (where the body is), setTargets (the pose, if it changed), step. Then
// segmentTransforms gives, for each segment, the transform in the body's frame of a model of that
// segment made at its bind place (the legs' bind joints, in the body's frame), and boneGlobals
// the world matrices of the segments ("leg<leg>_<segment>").
class InsectLegs : public ProceduralPose {
public:
  // The floor under (x, z): its height, false if there is none there
  typedef std::function<bool(float x, float z, float &height)> FloorQuery;
  typedef std::vector<std::vector<glm::vec3>> Pose; // per leg, its joints (hip first)

private:
  Pose bind, target;
  std::vector<std::vector<glm::vec3>> points, previous; // world
  std::vector<std::vector<float>> lengths;
  std::vector<glm::vec3> bindFlat;  // all the bind joints in a row (for rigBoneGlobals)
  std::vector<RigBone> bones;
  glm::mat4 body = glm::mat4(1.0f), lastBody = glm::mat4(1.0f);
  bool started = false;
  FloorQuery floor;
  InsectLegParams params;

  void flatten(const std::vector<std::vector<glm::vec3>> &from, std::vector<glm::vec3> &to) const;

public:
  // `bindLegs`: each leg's joints in the body's frame, as its models were made
  explicit InsectLegs(const Pose &bindLegs, const InsectLegParams &params = InsectLegParams());

  // (it may have a uniform scale: the legs are as much bigger or smaller)
  void setBody(const glm::mat4 &bodyToWorld);
  // The pose the legs go towards (body frame, same shape as the bind pose)
  void setTargets(const Pose &pose) { target = pose; }
  const Pose &getBind() const { return bind; }
  void setFloor(const FloorQuery &query) { floor = query; }
  // Puts every joint where the pose wants it, at rest (on the next step)
  void reset() { started = false; }

  void step(double dt) override;
  void boneGlobals(const glm::vec3 &origin,
                   std::map<std::string, glm::mat4> &out) const override;
  // For leg l and segment s (out[l * (joints - 1) + s]): the transform, in the body's frame, of
  // a model of that segment made at its bind place
  void segmentTransforms(std::vector<glm::mat4> &out) const;
  const std::vector<std::vector<glm::vec3>> &getPoints() const { return points; }
};

#endif
