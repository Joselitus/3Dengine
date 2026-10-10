#ifndef SHADE
#define SHADE

#include <functional>
#include <memory>
#include <random>
#include <vector>

#include "DynamicGameObject.h"
#include "ParticleEmitter.h"

// A shade: a night monster made of darkness (assets/shades, generate_shades.py), in one of VARIANTS
// shapes (wolf, stag, big cat, crawler, tall one, hunched one, goat man, crow man), black or nearly.
// It does not attack: at night it appears somewhere hidden from the player (behind a rock, a tree, a
// dune: the map's spot query) and follows him slowly, always keeping some distance (between KEEP_NEAR
// and KEEP_FAR): it comes nearer if he goes away and backs off if he comes, but slower than he walks,
// so he can get to it. When he is within VANISH_DISTANCE it turns into a cloud of dark smoke and is
// gone; whoever walks into that smoke (while it lasts: CLOUD_TIME) gets the grain of Bob's presence
// on his screen for a while (see VehicleStage::smokeHaze). At dawn it turns into smoke too.
// After it has gone it comes again, somewhere else and in another shape, AWAY_MIN..AWAY_MAX later.
//
// It goes through anything (it is not solid) and floats on the floor. The server moves it; the
// clients get its state (shape, what it does, for how long) and make the smoke themselves.
class Shade : public DynamicGameObject {
public:
  enum class State { Away, Following, Smoke };

  static constexpr int VARIANTS = 8;
  static constexpr float WALK_SPEED = 1.1f;       // m/s, coming nearer or backing off (a walking penguin: 4)
  static constexpr float KEEP_NEAR = 12.0f;       // m: nearer than this, it backs off
  static constexpr float KEEP_FAR = 20.0f;        // m: farther than this, it comes nearer
  static constexpr float VANISH_DISTANCE = 4.0f;  // m: this near, it turns into smoke
  static constexpr float LOSE_DISTANCE = 90.0f;   // m: this far (he drove away), it is gone (no smoke)
  static constexpr float SHRINK_TIME = 0.7f;      // s, the body melts into the smoke
  static constexpr float CLOUD_TIME = 12.0f;      // s the smoke lasts, from when it came
  static constexpr float CLOUD_RADIUS = 2.6f;     // m, round the place it was (walking in: the grain)
  static constexpr float AWAY_MIN = 15.0f, AWAY_MAX = 45.0f; // s gone, before it comes again
  static constexpr float RETRY_TIME = 3.0f;       // s, when there was no hidden place to come to

  // Where to come: a hidden place near some player, and that player's id (false: nowhere now)
  typedef std::function<bool(glm::vec3 &where, int &target)> SpotQuery;
  // Where the player with that id is (his feet); false if he is gone or dead
  typedef std::function<bool(int target, glm::vec3 &where)> TargetQuery;

private:
  State state = State::Away;
  float stateTime = 0.0f;
  float awayFor = 0.0f;   // (Away) s until it tries to come
  int variant = 0;
  int target = -1;        // the player it follows
  float yaw = 0.0f;
  float phase = 0.0f;     // (its slow swaying)
  std::mt19937 random;
  std::shared_ptr<ParticleEmitter> smoke;

  std::function<bool()> isNight;
  SpotQuery spot;
  TargetQuery where;
  std::function<bool(float, float, float &)> floorHeight;

  void enter(State next);
  void appear();
  void dissolve();
  void goAway();
  void show();       // its shape, size and visibility, as its state says
  void updateSmoke(double dt);

public:
  // `shapes`: the VARIANTS models (shade_0..7.obj); `seed` sets it apart from the others
  Shade(const std::vector<std::shared_ptr<Model>> &shapes, unsigned seed);

  void setNightQuery(std::function<bool()> q) { isNight = q; }
  void setSpotQuery(SpotQuery q) { spot = q; }
  void setTargetQuery(TargetQuery q) { where = q; }
  void setFloorQuery(std::function<bool(float, float, float &)> floor) { floorHeight = floor; }
  // Waits this long before coming the first night (the map sets them apart)
  void setFirstDelay(float seconds) { awayFor = seconds; }

  State getState() const { return state; }
  // The smoke is there (to walk into): it came less than CLOUD_TIME ago, round getPosition()
  bool hasCloud() const { return state == State::Smoke && stateTime < CLOUD_TIME; }
  // Its smoke: the map adds the emitter to its own
  std::shared_ptr<ParticleEmitter> getSmoke() const { return smoke; }

  void update(double dt) override;
  void writeNetState(NetWriter &out) const override;
  void readNetState(NetReader &in) override;
  bool contactFloor(const Stage &stage, double dt) override { return true; } // (it keeps itself on it)
  float getHeading() const override { return yaw; }
  void describe(std::vector<std::string> &lines) const override;
};

#endif
