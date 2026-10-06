#ifndef PROCEDURALPOSE
#define PROCEDURALPOSE

#include <map>
#include <string>

#include <glm/glm.hpp>

// A skeleton pose that code makes every frame instead of an authored animation: a ragdoll, an IK
// reach, a procedural walk... It is stepped in time and then gives the world matrix of each bone
// (by name, relative to `origin`), which is what AnimatedModel::setBoneGlobals takes. Pure
// maths, no OpenGL. Each subclass has its own setup (starting state, what it collides with...)
// outside this interface; whoever owns it only needs to step it and read the pose.
class ProceduralPose {
public:
  virtual ~ProceduralPose() {}

  // Advances the pose by dt seconds
  virtual void step(double dt) = 0;
  // The world matrix of every bone it poses, by bone name, relative to `origin`
  virtual void boneGlobals(const glm::vec3 &origin,
                           std::map<std::string, glm::mat4> &out) const = 0;
  // True once it will not change any more (a ragdoll that fell asleep): it can stop being
  // stepped. By default it never finishes.
  virtual bool isFinished() const { return false; }
};

#endif
