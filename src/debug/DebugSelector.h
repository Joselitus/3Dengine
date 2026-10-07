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

// Debug modes to inspect the objects of the map, change their values and move
// them around. They show a crosshair at the centre of the view and a box of data at the top
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
//   (Stage::relocate), keeping its height above the floor. Holding the right
//   button and moving the mouse sideways turns it around the vertical
//   (Stage::turn) instead of the camera (see capturesMouse); with Shift held
//   as well, its heading snaps to multiples of 15 degrees.
// - Properties (Action::DebugInspect, 0 by default): the object the crosshair
//   points at (no click needed) is outlined and its values
//   (GameObject::getProperties: the RV's speed, the chance its headlights
//   fail...) are listed in the box. A left click on it opens a window to change
//   them (PropertyPanel); a right click, the player's. Pointing at no object,
//   the box shows the world's (Stage::getProperties: the time of day and how
//   fast it runs), and a left click opens them.
// Each key turns its mode on (or off, if it is the current one).
//
// It is not a panel, just an overlay (UIOverlay): the player keeps moving and
// looking around meanwhile. Call update() every frame and draw() after the
// world; clear() before the map is destroyed (it only keeps a weak pointer,
// but the selection belongs to the old map).
class DebugSelector : public UIOverlay {
public:
  enum class Mode { Off, Select, Place, Inspect };

private:
  GLFWwindow *window;
  UIManager &ui;
  const Controls &controls;
  LineRenderer lines;
  Mode mode = Mode::Off;
  bool leftWasDown = false, rightWasDown = false;
  std::weak_ptr<GameObject> selected;
  std::string selectedName;
  std::weak_ptr<GameObject> hovered; // Inspect: under the crosshair
  std::string hoveredName;
  std::vector<std::string> info; // what the box shows, refreshed each update
  // Frames per second, shown in the selection mode: frames and time counted over
  // FPS_PERIOD seconds, then the shown value is renewed
  static constexpr double FPS_PERIOD = 0.5;
  int fpsFrames = 0;
  double fpsTime = 0.0;
  float fps = 0.0f;
  bool turning = false;          // Place: the right button turns the object
  double lastCursorX = 0.0;      // while turning, on the previous update
  float turnHeading = 0.0f;      // while turning, where the mouse has taken
                                 // it (radians, before snapping)
  bool snapping = false;         // while turning, Shift is held
  bool hasTarget = false;        // Place: the crosshair points at the floor
  glm::vec3 target;              // Place: where the object would go

  // The object the ray hits first (null if none, or the floor is in front)
  std::shared_ptr<GameObject> pick(const GameStage &stage,
                                   const glm::vec3 &origin,
                                   const glm::vec3 &direction) const;
  void select(const GameStage &stage, std::shared_ptr<GameObject> object);
  // Its name, plus its index in the stage, which tells apart objects of the
  // same class (e.g. "GameObject #12")
  static std::string labelOf(const GameStage &stage, const GameObject &object);
  // Inspect: opens the window to change the values of `object`
  void edit(const GameStage &stage, std::shared_ptr<GameObject> object);
  // Inspect: opens the window to change the world's values
  void editWorld(GameStage &stage);
  // Places a properties window at the right, leaving the middle of the view
  // in sight
  void placeAtRight(UIPanel *panel);
  // Where `object` would go if the crosshair's floor point is `floorPoint`
  glm::vec3 destination(const GameStage &stage, const GameObject &object,
                        const glm::vec3 &floorPoint) const;
  void refresh(GameStage &stage, Camera &camera);
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
  void toggleInspect() {
    setMode(mode == Mode::Inspect ? Mode::Off : Mode::Inspect);
  }
  Mode getMode() const { return mode; }
  void turnOff() { setMode(Mode::Off); }
  // Whether the mouse turns the selected object now (placement mode, right
  // button held): then the camera must not turn with it
  // (Controller::setLookEnabled)
  bool capturesMouse() const;
  // Forgets the selected and the hovered object (e.g. its map is about to be
  // replaced)
  void clear();

  // Counts a frame that took `dt` seconds (call it every frame): the selection mode shows the
  // frames per second
  void countFrame(double dt) {
    fpsFrames++;
    fpsTime += dt;
    if (fpsTime >= FPS_PERIOD) {
      fps = (float)(fpsFrames / fpsTime);
      fpsFrames = 0;
      fpsTime = 0.0;
    }
  }

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
