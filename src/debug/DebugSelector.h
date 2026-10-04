#ifndef DEBUG_SELECTOR
#define DEBUG_SELECTOR

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "CollisionShape.h"
#include "Controls.h"
#include "LineRenderer.h"
#include "UIManager.h"
#include "UIOverlay.h"

class Camera;
class GameObject;
class GameStage;

// Debug modes to inspect the objects of the map and move them around. Both
// show a crosshair at the centre of the view and a box of data at the top
// left, and draw the selected object's collision shape (orange), its
// axis-aligned bounding box (blue) and its velocity (green) over the world.
//
// - Selection (Action::DebugSelect, 1 by default): a left click selects the
//   object the crosshair points at (the nearest visible, collidable one the
//   ray from the camera hits, unless a dune is in the way), a right click the
//   player itself. The box shows its data (GameObject::describe: position,
//   rotation, collision shape, bounding box, velocity, mass...; plus the floor
//   under it), refreshed every frame.
// - Placement (Action::DebugPlace, 2 by default): the point of the floor the
//   crosshair points at is the destination, where a white outline of the
//   selected object is drawn; a left click moves the object there
//   (Stage::relocate), keeping its height above the floor. A right click goes
//   back to selecting.
// Each key turns its mode on (or off, if it is the current one).
//
// It is not a panel, just an overlay (UIOverlay): the player keeps moving and
// looking around meanwhile. Call update() every frame and draw() after the
// world; clear() before the map is destroyed (it only keeps a weak pointer,
// but the selection belongs to the old map).
class DebugSelector : public UIOverlay {
public:
  enum class Mode { Off, Select, Place };

private:
  GLFWwindow *window;
  UIManager &ui;
  const Controls &controls;
  LineRenderer lines;
  Mode mode = Mode::Off;
  bool leftWasDown = false, rightWasDown = false;
  std::weak_ptr<GameObject> selected;
  std::string selectedName;
  std::vector<std::string> info; // what the box shows, refreshed each update
  bool hasTarget = false;        // Place: the crosshair points at the floor
  glm::vec3 target;              // Place: where the object would go

  // The object the ray hits first (null if none, or the floor is in front)
  std::shared_ptr<GameObject> pick(const GameStage &stage,
                                   const glm::vec3 &origin,
                                   const glm::vec3 &direction) const;
  void select(const GameStage &stage, std::shared_ptr<GameObject> object);
  // Where `object` would go if the crosshair's floor point is `floorPoint`
  glm::vec3 destination(const GameStage &stage, const GameObject &object,
                        const glm::vec3 &floorPoint) const;
  void refresh(const GameStage &stage, Camera &camera);
  void setMode(Mode mode);
  // Outline of `shape` at `pose`
  void outline(const CollisionShape &shape, const Pose &pose,
               const glm::vec4 &color);

public:
  DebugSelector(GLFWwindow *window, UIManager &ui, const Controls &controls);
  ~DebugSelector();

  // The keys of the modes: on, or off if it was already on
  void toggleSelect() { setMode(mode == Mode::Select ? Mode::Off : Mode::Select); }
  void togglePlace() { setMode(mode == Mode::Place ? Mode::Off : Mode::Place); }
  Mode getMode() const { return mode; }
  // Forgets the selected object (e.g. its map is about to be replaced)
  void clear();

  // Reads the mouse (if `canPick`: no panel is open), selects or moves, and
  // refreshes the data
  void update(GameStage &stage, Camera &camera, bool canPick);
  // The selected object's shape and bounds (and where it would go), over the
  // world
  void draw(Camera &camera);
  // UIOverlay: the crosshair and the box of data
  void draw(UIRenderer &renderer, float width, float height) const override;

  // Readable name of an object: its class, and its name if it has one (e.g.
  // "Npc 'Pingu'")
  static std::string nameOf(const GameObject &object);
  // The first point of the floor along the ray (unit direction), within
  // `maxDistance`; false if it doesn't reach the floor
  static bool floorHit(const GameStage &stage, const glm::vec3 &origin,
                       const glm::vec3 &direction, float maxDistance,
                       glm::vec3 &point);
};

#endif
