#ifndef SAFE_SPACE
#define SAFE_SPACE

#include <memory>

#include <glm/glm.hpp>

class GameObject;

// A place where the player is out of the enemies' reach (inside the RV...): while he is in one,
// the map tells its creatures he is sheltered and they behave as when he drives (they keep away,
// they can't catch, bite or grab him). A stage keeps its safe spaces (Stage::addSafeSpace) and
// answers Stage::isSheltered.
//
// It is a box (centre and half size) in the frame of the object that carries it, so it moves and
// turns with it (the RV's cabin); without an object it stays fixed in the world (a hut...). A
// subclass may override contains() for another shape, or to shelter only sometimes.
class SafeSpace {
  std::weak_ptr<const GameObject> owner;
  bool carried = false;
  glm::vec3 centre, halfSize;

public:
  // A box in `carrier`'s frame (the safe space goes when it does)
  SafeSpace(std::shared_ptr<const GameObject> carrier, const glm::vec3 &centre, const glm::vec3 &halfSize);
  // A box fixed in the world
  SafeSpace(const glm::vec3 &centre, const glm::vec3 &halfSize);
  virtual ~SafeSpace() = default;

  // Whether `point` (the player's feet) is inside it
  virtual bool contains(const glm::vec3 &point) const;
};

#endif
