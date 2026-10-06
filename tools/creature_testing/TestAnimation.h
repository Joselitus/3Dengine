#ifndef TEST_ANIMATION
#define TEST_ANIMATION

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

class Stage;

// Where the creature's body is, which the tool moves (drags it, walks it to a point) and the
// animation reads. An animation that moves the body by itself (a ragdoll falls) changes it too.
struct TestBody {
  glm::vec3 position = glm::vec3(0.0f); // on the ground, under the body
  float heading = 0.0f;                 // radians about +y; 0 = facing +z
  glm::vec3 velocity = glm::vec3(0.0f); // how fast it really moves (measured by the tool)
  bool held = false;                    // the mouse has it
  glm::vec3 viewer = glm::vec3(0.0f);   // where the camera is (a face may look at it)
};

// One animation of one creature, as the creature testing tool shows it: the creature's object is
// in the stage (the animation put it there) and the animation poses it every frame.
class TestAnimation {
public:
  virtual ~TestAnimation() {}
  virtual std::string name() const = 0;
  // Starts again with the body as it is now
  virtual void reset(const TestBody &body) = 0;
  // One frame: moves and poses the creature's object from the body
  virtual void update(double dt, TestBody &body) = 0;
  // Where its joints are now, in the world: what the mouse can grab it by
  virtual void joints(std::vector<glm::vec3> &out) const = 0;
  // The bones of its skeleton as they are now (pairs of world points), for the tool to draw over it
  // (key K): empty if the animation has no skeleton of its own
  virtual void skeleton(std::vector<std::pair<glm::vec3, glm::vec3>> &out) const {}
  // Whether the creature can be sent to a point (a ragdoll can't walk)
  virtual bool walks() const { return true; }
  // A line about what it is doing now
  virtual std::string status() const { return ""; }
};

// A creature the tool can test and the animations it has. `create` puts the creature in the
// stage, showing animation `index`.
struct CreatureEntry {
  std::string name;
  std::vector<std::string> animations;
  float walkSpeed = 4.0f; // m/s, when it is sent to a point
  std::function<std::unique_ptr<TestAnimation>(int index, Stage &stage)> create;
};

// The creatures that can be tested: one CreatureEntry each (see FollaCulosTests.cpp), listed by
// creatureEntries() in Creatures.cpp
CreatureEntry follaCulosEntry();
CreatureEntry pinguEntry();
std::vector<CreatureEntry> creatureEntries();

#endif
