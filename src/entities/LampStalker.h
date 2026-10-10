#ifndef LAMP_STALKER
#define LAMP_STALKER

#include <functional>
#include <memory>
#include <random>
#include <vector>

#include "DynamicGameObject.h"

// The living lamp post (an enemy): it looks exactly like the desert's lamp posts (LampPost, the same
// models) and stands among them. Under its footing it has four tiny pale feet, drawn up inside it
// while it stands (they do not show).
//
//   Pole --(a player passes in front of it: within TRIGGER_RANGE, on the side its arm points to)-->
//     on foot (and out in the open): Chase. It pushes its feet out, rises on them and runs at him on
//       its tiny feet (very fast steps), leaning forward; within KILL_DISTANCE it kills him (the
//       stage's callback). It only kills a player out in the open: not in the RV (driving it or on
//       foot inside it: the stage's "sheltered").
//     in the RV (or in another safe place): Hide.
//   Hide: it keeps away from him (KEEP_NEAR..KEEP_FAR) and follows him from one hiding place to the
//     next: a place in line with another lamp post (LampPost::lineSpot: it passes for one more of the
//     row), the one nearest to IDEAL_DISTANCE from him; there it draws its feet in and turns like that
//     post. If there are no other posts it just stands where it is. If he comes out in the open
//     within CHASE_AGAIN: Chase.
//   Chase --(it has killed him)--> / Chase, Hide --(nobody alive within LOSE_RANGE)--> Return: back to
//     the place it was put in (setYaw/teleport: its home), and there Pole again.
//   Two shots of the alien ship's ray (takeDamage) kill it: it falls over (Dead).
//
// The server moves it; the clients get its state and how far its feet are out (writeNetState) and
// move the feet themselves.
class LampStalker : public DynamicGameObject {
public:
  enum class State { Pole, Chase, Hide, Return, Dead };

  static constexpr float TRIGGER_RANGE = 15.0f; // m
  static constexpr float FRONT_DOT = 0.2f;      // in front: cos of the angle from its arm's way
  static constexpr float RUN_SPEED = 6.5f;      // m/s (a penguin walks at 4, runs at 7)
  static constexpr float KILL_DISTANCE = 1.2f;  // m
  static constexpr float KEEP_NEAR = 25.0f, KEEP_FAR = 60.0f, IDEAL_DISTANCE = 35.0f; // m (Hide)
  static constexpr float CHASE_AGAIN = 60.0f;   // m
  static constexpr float LOSE_RANGE = 150.0f;   // m
  static constexpr float REPLAN_TIME = 2.0f;    // s between looks for a better hiding place
  static constexpr float FEET_RATE = 3.0f;      // 1/s, the feet going out or in
  static constexpr float TURN_RATE = 5.0f;      // rad/s
  static constexpr float FALL_TIME = 1.2f;      // s, falling over dead
  static constexpr float DETOUR_TIME = 1.2f;    // s, going round what it ran into (sideways)

  struct Victim {
    int id;
    glm::vec3 position; // (his feet)
    bool sheltered;     // in the RV, or another safe place: it can't kill him
  };
  // A place to hide: in line with a post, turned like it
  struct Spot {
    glm::vec3 position;
    float yaw;
  };

private:
  State state = State::Pole;
  float yaw = 0.0f;
  float extend = 0.0f;   // 0 feet in .. 1 feet out (it stands on them)
  float phase = 0.0f;    // of its steps
  float health = 1.0f;
  float fall = 0.0f;     // (Dead) 0 standing .. 1 lying
  float replanIn = 0.0f;
  bool hasSpot = false;  // (Hide) it has a place to go to / it is at
  Spot spot{glm::vec3(0.0f), 0.0f};
  Spot home{glm::vec3(0.0f), 0.0f}; // where it was put: it goes back there (Return)
  int target = -1;
  float stuck = 0.0f;        // s it has hardly moved while it wants to (something in the way)
  float detour = 0.0f;       // s left going round it, to one side (detourSide +1 / -1)
  float detourSide = 1.0f;
  size_t glassPart = 0;
  size_t feet[4] = {0, 0, 0, 0};

  std::function<std::vector<Victim>()> victims;
  std::function<std::vector<Spot>()> spots;
  std::function<void(int)> kill;

  void enter(State next);
  bool findTarget(Victim &victim) const;
  void chooseSpot(const glm::vec3 &from);
  void walkTo(const glm::vec3 &where, float speed, double dt);
  void standStill();
  void pose(double dt); // the body, the glass and the feet

public:
  // `post`, `glass`: the lamp post's own models (it looks just like one); `foot`: lamppost_foot.obj
  LampStalker(std::shared_ptr<Model> post, std::shared_ptr<Model> glass, std::shared_ptr<Model> foot);

  // The players it may go for (alive)
  void setVictimsQuery(std::function<std::vector<Victim>()> q) { victims = q; }
  // The places it can hide in (in line with the other posts)
  void setSpotsQuery(std::function<std::vector<Spot>()> q) { spots = q; }
  // What it does to the one it reaches (the stage kills him)
  void setKillCallback(std::function<void(int)> k) { kill = k; }
  void setLit(bool lit) { setPartUnlit(glassPart, lit && state != State::Dead ? 2 : 0); }

  State getState() const { return state; }
  void setYaw(float y);
  float getHeading() const override { return yaw; }
  void turn(float radians) override { setYaw(yaw + radians); }
  // (where it is put, and how it is turned, is its home: see Return)
  void teleport(const glm::vec3 &where) override;
  void update(double dt) override;
  void takeDamage(float amount, const glm::vec3 &direction, const Stage &stage) override;
  void writeNetState(NetWriter &out) const override;
  void readNetState(NetReader &in) override;
  void describe(std::vector<std::string> &lines) const override;
};

#endif
