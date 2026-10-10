#ifndef HOUSE
#define HOUSE

#include <memory>

#include "CollisionShape.h"
#include "DynamicGameObject.h"
#include "Interactable.h"

// The house (assets/house, prepare_house.py, from the user's Blender model): a room on a concrete
// block, its door on +x up four steps from a porch under a roof on eight beams. It is scenery (a
// static object: the map editor places it like any prop, see PropCatalog) whose collision shape is
// made of boxes, hollow: one can climb the steps, go in through the doorway and walk round the bed.
// The door is a part of it, swung by setDoorAngle; shut, its box is a wall.
//
// In the game the door is used and kept in step by a HouseDoor (GameStage::addHouseDoor): the house
// itself is not sent over the network (it is static), the door's state is.
class House : public GameObject {
  std::shared_ptr<CompoundShape> hull;
  size_t doorPart = 0;
  float doorAngle = 0.0f;
  House(std::shared_ptr<Model> model, std::shared_ptr<Model> door, std::shared_ptr<CompoundShape> hull);

public:
  static constexpr float DOOR_OPEN_ANGLE = 1.75f; // radians (100 degrees, outwards onto the porch)

  // `door`: house_door.obj (in the frame of its hinge)
  House(std::shared_ptr<Model> model, std::shared_ptr<Model> door);
  static std::shared_ptr<CompoundShape> makeHull();
  // The ground it stands on, in its frame (x and z: the block, the porch and the steps; one metre more)
  static void footprint(glm::vec3 &low, glm::vec3 &high);

  // 0 shut, DOOR_OPEN_ANGLE open
  void setDoorAngle(float angle);
  float getDoorAngle() const { return doorAngle; }
  // Where to stand to use the door: on the top step, in front of it
  glm::vec3 doorPoint() const;
};

// The house's door in the game: it opens or shuts with E (all the way, at a hand's pace), on the
// server, and the clients get its state (it is a dynamic object with nothing to show and no body:
// it is how the door's state travels; its house draws the door).
class HouseDoor : public DynamicGameObject, public Interactable {
  std::shared_ptr<House> house;
  bool open = false;
  float angle = 0.0f;

public:
  static constexpr float SPEED = 2.5f; // rad/s

  HouseDoor(std::shared_ptr<Model> model, std::shared_ptr<House> house);

  bool isOpen() const { return open; }
  void update(double dt) override;
  bool contactFloor(const Stage &stage, double dt) override { return true; } // (it stays where it is)
  void writeNetState(NetWriter &out) const override;
  void readNetState(NetReader &in) override;

  std::string getInteractionName() const override { return "puerta"; }
  std::string getInteractionVerb() const override { return open ? "cerrar la" : "abrir la"; }
  glm::vec3 getInteractionPoint() const override { return house->doorPoint(); }
  float getInteractionRange() const override { return 1.6f; }
  bool usesDirectly() const override { return true; }
  void onUse(const glm::vec3 &) override { open = !open; }
  void buildInterface(UIPanel &) override {}
};

#endif
