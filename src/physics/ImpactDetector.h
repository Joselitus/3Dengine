#ifndef IMPACT_DETECTOR
#define IMPACT_DETECTOR

#include <glm/glm.hpp>

// What counts as a violent frontal crash (see ImpactDetector); a struct of its own so that it
// can be a default argument
struct ImpactParams {
  float window = 0.25f;     // seconds the timer runs after a frontal collision
  float minSpeed = 5.0f;    // m/s: slower than this is a bump, not a crash
  float minDrop = 6.0f;     // m/s the speed has to fall at least...
  float decel = 48.0f;      // ...and at least this fast (m/s^2; the RV's brakes: 16)
  float frontalCos = 0.6f;  // the hit is frontal if the push is within ~53 degrees of straight back
};

// Tells a violent frontal crash from everything else that can happen to a vehicle (a
// bump, a graze, a push from the side, hard braking): a crash is when it hits something
// in front of it while it is moving and, within a very short time of the hit, its speed
// drops far faster than it ever would braking.
//
// The vehicle tells it about each collision (onCollision) and about its speed every frame
// (update). A frontal collision at some speed starts a short timer; if inside it the speed
// falls by at least `minDrop`, and faster than `decel` (m/s^2: well above what the brakes
// give), the impact is violent (violent()) until clear() is called. Pure logic, no
// OpenGL: see RV, which breaks its windshield with it.
class ImpactDetector {
public:
  explicit ImpactDetector(const ImpactParams &params = ImpactParams()) : params(params) {}

  // A collision: the vehicle was going `speedBefore` along its heading (m/s, forward is
  // positive) and now goes `speedAfter`. `heading` is where it faces and `away` where the
  // collision pushed it (the way from what it hit); only their horizontal parts count.
  void onCollision(float speedBefore, float speedAfter, const glm::vec3 &heading,
                   const glm::vec3 &away);
  // Every frame, with the vehicle's speed along its heading now
  void update(float dt, float forwardSpeed);

  bool timerRunning() const { return running; }
  float timeSinceImpact() const { return elapsed; }
  // A violent frontal impact has happened (it stays true until clear())
  bool violent() const { return wasViolent; }
  void clear() {
    wasViolent = false;
    running = false;
  }

private:
  ImpactParams params;
  bool running = false;
  float elapsed = 0.0f;
  float speedAtImpact = 0.0f;
  bool wasViolent = false;

  void check(float speedNow);
};

#endif
