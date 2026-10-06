#ifndef NPC_RAGDOLL
#define NPC_RAGDOLL

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "AnimatedModel.h"
#include "Ragdoll.h"

// A ragdoll for a humanoid skinned model (the NPCs: Pingu), made from its own skeleton: the joints
// of its main bones (spine, neck, head, arms, legs; the names of the Unreal mannequin the penguin
// is rigged with) at the places the model's bind pose has them, with the same links as the
// creature's (a rigid torso, limbs that can't fold right back). The rest of its bones (fingers,
// twist bones, hair) follow the ones they hang from (Skeleton::SetPose). It works in the world,
// in metres (the model's own units are fitted to the object: AnimatedModel::fitCenter and
// fitScale), and poses the model every step. Its head can be held at a point of the world (a
// mouth): the rest of it then hangs from the head.
class NpcRagdoll {
public:
  explicit NpcRagdoll(std::shared_ptr<AnimatedModel> model);

  // False if the model lacks the bones this needs
  bool isValid() const { return valid; }
  // Starts it from the pose the model has now, for an object at `position` turned by `rotation`
  // (the skeleton as the animation last left it), moving at `velocity`
  void start(const glm::vec3 &position, const glm::mat3 &rotation, const glm::vec3 &velocity);
  // Holds the head at this world point (call it every frame) / lets it go
  void holdHead(const glm::vec3 &world) { ragdoll->pin(HEAD, world); }
  void releaseHead() { ragdoll->unpin(); }
  void step(double dt, const Ragdoll::FloorQuery &floor, const Ragdoll::PushOut &pushOut) {
    ragdoll->step(dt, floor, pushOut);
  }
  // Where its pelvis is, in the world: the object goes there
  const glm::vec3 &pelvis() const { return ragdoll->getPoints()[PELVIS]; }
  const std::vector<glm::vec3> &points() const { return ragdoll->getPoints(); }
  void segments(std::vector<std::pair<glm::vec3, glm::vec3>> &out) const { ragdoll->segments(out); }
  // Poses the model for an object at `origin` with no rotation
  void apply(const glm::vec3 &origin);

private:
  enum Point { PELVIS, SPINE1, SPINE2, SPINE3, SPINE4, SPINE5, NECK1, NECK2, HEAD, CLAV_L, CLAV_R,
               UPPER_L, UPPER_R, LOWER_L, LOWER_R, HAND_L, HAND_R, HAND_TIP_L, HAND_TIP_R, THIGH_L,
               THIGH_R, CALF_L, CALF_R, FOOT_L, FOOT_R, BALL_L, BALL_R, BALL_TIP_L, BALL_TIP_R, POINTS };
  std::shared_ptr<AnimatedModel> model;
  std::unique_ptr<Ragdoll> ragdoll;
  bool valid = false;
  std::vector<glm::vec3> bindRaw;    // each point in the model's own units, bind pose
  std::vector<std::string> pointBone; // the bone each point belongs to (it is that bone's joint, or a point beyond it)
  std::vector<glm::vec3> pointJoint;  // ...and where that bone's joint is, bind pose
  glm::vec3 toMetres(const glm::vec3 &raw) const;
};

#endif
