#ifndef FLATWOODS
#define FLATWOODS

#include <functional>
#include <memory>
#include <random>

#include "DynamicGameObject.h"

// The Flatwoods monster (assets/flatwoods, generate_flatwoods.py): a tall floating figure in a dark
// pleated skirt, with a spade-shaped hood and red eyes. It can only be seen in mirrors: it is
// drawn only while the main loop draws the RV's rear-view mirrors (setMirrorView), never in the
// player's own view.
//
// It only comes at night, and only for a player who is in the RV (driving it, or on foot inside:
// the map's query); otherwise it hangs in the air where it is, watching. It floats straight at
// him at a slow, steady FLOAT_SPEED, through anything (the RV's walls too: it is not solid), whether he
// drives or not. The first time each night it comes from behind the RV (a little to the driver's
// side, BEHIND_SKEW, so that the driver's mirror shows it), APPEAR_DISTANCE away;
// after that, from a new side each time (at least MIN_TURN away from the last one).
//
// When it reaches him (CATCH_DISTANCE) it takes hold of him (the map's possess callback: his body
// walks out of the RV) and is not there any more. When he breaks free (release), or when his
// flashlight shines on it (the map's query), it vanishes, and after COME_BACK_TIME it comes again
// from a new side. At dawn it goes, and the next night starts again from behind the RV.
class Flatwoods : public DynamicGameObject {
public:
  enum class State { Away, Waiting, Coming, Holding };

  static constexpr float FLOAT_SPEED = 2.0f;      // m/s (not SPEED: a macro of Camera.h)
  static constexpr float APPEAR_DISTANCE = 35.0f; // m from the RV
  static constexpr float CATCH_DISTANCE = 1.0f;   // m, across
  static constexpr float HOVER = 0.35f;           // m over the floor, the hem of its skirt
  static constexpr float COME_BACK_TIME = 6.0f;   // s, gone after a flashlight or a release
  static constexpr float MIN_TURN = 1.57f;        // rad, between one side and the next
  // The first time, behind the RV but this much (rad) towards its driver's side (+x): straight
  // behind, its own body would hide it from the side mirrors
  static constexpr float BEHIND_SKEW = 0.35f;

private:
  State state = State::Away;
  float stateTime = 0.0f;
  bool cameTonight = false;    // it has come once this night (from behind)
  bool mirrorView = false;     // the mirrors are being drawn
  float side = 0.0f;           // the side it came from last (rad about +y, from the RV)
  float yaw = 0.0f;
  float phase = 0.0f;          // (its floating up and down)
  std::mt19937 random;

  std::function<bool()> isNight, playerInRV;
  std::function<glm::vec3()> target;     // the player's feet
  std::function<void(glm::vec3 &, glm::vec3 &)> vehicle; // the RV's position and forward (+z)
  std::function<bool(float, float, float &)> floorHeight;
  std::function<bool(const glm::vec3 &)> lit; // the player's flashlight shines on that point
  std::function<void()> possess;

  void appear(bool fromBehind);
  void vanish();
  void enter(State next);

public:
  // `body`: skirt, chest, arms, face and hood; `eyes`: its eyes (drawn glowing)
  Flatwoods(std::shared_ptr<Model> body, std::shared_ptr<Model> eyes);

  void setNightQuery(std::function<bool()> q) { isNight = q; }
  void setPlayerInRVQuery(std::function<bool()> q) { playerInRV = q; }
  void setTarget(std::function<glm::vec3()> where) { target = where; }
  void setVehicleFrame(std::function<void(glm::vec3 &, glm::vec3 &)> frame) { vehicle = frame; }
  void setFloorQuery(std::function<bool(float, float, float &)> floor) { floorHeight = floor; }
  void setFlashlightQuery(std::function<bool(const glm::vec3 &)> q) { lit = q; }
  // What happens when it reaches the player (the map: it takes his body out of the RV)
  void setPossessCallback(std::function<void()> callback) { possess = callback; }

  // The player broke free: it goes, and comes again from a new side
  void release();
  // The main loop draws the mirrors (true) or the player's view (false): it shows only in mirrors
  void setMirrorView(bool inMirror);
  State getState() const { return state; }
  bool isHolding() const { return state == State::Holding; }

  void update(double dt) override;
  bool contactFloor(const Stage &stage, double dt) override { return true; } // (it floats)
  float getHeading() const override { return yaw; }
  void describe(std::vector<std::string> &lines) const override;
};

#endif
