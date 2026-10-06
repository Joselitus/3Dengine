#ifndef SPIDERGAIT
#define SPIDERGAIT

#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "PointRig.h"
#include "ProceduralPose.h"

// One limb of the gait: the points of its joints in the rig (the root is where it hangs from the
// body, the tip is what touches the ground), whether it is a front one, and its side (+1 = +x).
struct SpiderLeg {
  int root, mid, end, tip;
  bool front;
  float side;
};

// The skeleton of the walker: the points of the spine in order from the hind end to the head's
// top, the points that go with a part of the spine (shoulders with the chest, hips with the
// pelvis: `attach[i]` is a point and `attachTo[i]` the index in `spine` it hangs from) and the legs.
struct SpiderRig {
  std::vector<int> spine;
  // The bones that turn to look at something (setLookTarget): the neck and the head, and the
  // indices in `spine` of the points they start at
  std::string neckBone, headBone;
  int neckSpine = -1, headSpine = -1;
  std::vector<int> attach, attachTo;
  std::vector<SpiderLeg> legs;
};

// How it walks (a top-level struct because of the default arguments)
struct SpiderParams {
  float bodyHeight = 0.55f;     // of the hind end of the spine above the ground
  float torsoFollow = 8.0f;     // 1/s: how fast the torso moves to the middle of the four feet
  float torsoBias = 0.435f;     // how far behind the middle of the feet the torso sits (m): closer to the hind feet
  float maxTorsoShift = 0.35f;  // ...and how far from the body's position it may go
  float lookRate = 8.0f;        // 1/s: how fast the face turns to what it looks at
  float maxTwist = 200.0f;      // degrees the head may twist on its neck (an owl's: more than half a turn)
  float maxTilt = 35.0f;        // degrees it may nod up or down on top of that, to look right at the target
  float neckShare = 0.4f;       // how much of the turn the neck does (the head does the rest)
  // How far each spine segment leans from the vertical, degrees (the last ones are neck and head)
  std::vector<float> lean = {80.0f, 78.0f, 75.0f, 62.0f, 48.0f, 32.0f};
  float frontRestX = 0.72f, frontRestZ = 1.23f; // where each foot "wants" to be, from the body
  float hindRestX = 0.72f, hindRestZ = -0.50f;
  float stepThreshold = 0.34f;  // a foot lifts when the body has gone this far past it (moving)
  float idleThreshold = 0.12f;  // ...and when it stands still (so the feet gather in)
  float leadTime = 0.05f;       // seconds of walking the foot lands ahead of its rest place
  float liftHeight = 0.24f;     // of a foot in the air
  float tipHeight = 0.045f;     // of the tip above the ground when it is planted
  float frontClaw = 0.0f, hindClaw = 0.17f; // how steeply the last bone points down (0: the hand is flat)
  float frontTipHeight = 0.30f; // how high the front hands' fingertips are: the whole flat hand stays above the floor
  // The vibration of the body: how hard each foot landing hits (a kick of speed to the spring)
  float bobKick = 0.5f, pitchKick = 0.9f, rollKick = 0.8f, headKick = 0.7f;
};

// A creature that walks on four legs like a spider, made by code: the body is carried over
// four feet that stay planted on the ground, and when the body has gone too far past a foot
// (SpiderParams::stepThreshold) that foot lifts, swings forward in an arc and lands ahead, so the
// legs together make the look of walking. Diagonal legs go together (a trot): a foot does not
// lift while a leg of the other pair is in the air, unless it is far behind. Two-bone IK gives the
// knees and elbows. Every landing (and lift) kicks springs of the torso and of the head, so they
// shake a little with each step. The torso stays over the middle of the four feet, and the face
// turns to look at a point (the camera: setLookTarget) like an owl: the head twists about its own
// axis, as far as more than half a turn, so it stays upright when it looks behind it. It is told where the body is each frame (setBody), steps, and
// gives the bones of the skeleton in the creature's own frame (origin at the body's position on
// the ground, +z forward), which the creature's rotation and position then place in the world.
// Pure maths, no OpenGL.
class SpiderGait : public ProceduralPose {
public:
  // The ground under (x, z): its height, false if there is none there
  typedef std::function<bool(float x, float z, float &height)> FloorQuery;

  SpiderGait(const std::vector<glm::vec3> &bindPoints, const std::vector<RigBone> &bones,
             const SpiderRig &rig, const SpiderParams &params = SpiderParams());

  // Where the body is now: its position on the ground, its axes (the creature's rotation: the
  // columns are its right, up and forward in the world) and how fast it moves
  void setBody(const glm::vec3 &position, const glm::mat3 &axes, const glm::vec3 &velocity) {
    this->position = position;
    this->axes = axes;
    this->velocity = velocity;
  }
  void setFloor(const FloorQuery &query) { floor = query; }
  // The face turns to look at this point of the world (e.g. the camera), as far as the neck
  // allows, and keeps doing it every frame until clearLookTarget()
  void setLookTarget(const glm::vec3 &world) {
    lookTarget = world;
    looking = true;
  }
  void clearLookTarget() { looking = false; }
  // Puts the feet back at their rest places the next step (after the body was moved)
  void reset() { placed = false; }

  void step(double dt) override;
  void boneGlobals(const glm::vec3 &origin,
                   std::map<std::string, glm::mat4> &out) const override;

  // For tests and the debug view
  const std::vector<glm::vec3> &getPoints() const { return points; } // the body's frame
  size_t legCount() const { return legs.size(); }
  const glm::vec3 &getFoot(size_t leg) const { return legs[leg].foot; } // in the world
  bool isSwinging(size_t leg) const { return legs[leg].swinging; }
  float getTorsoPitch() const { return pitch.x; }
  float getBob() const { return bob.x; }

private:
  struct Spring {
    float x = 0.0f, v = 0.0f;
    void step(float dt, float omega, float zeta) {
      for (int i = 0; i < 2; i++) { // (two sub-steps keep it stable at a low frame rate)
        v += (-omega * omega * x - 2.0f * zeta * omega * v) * dt * 0.5f;
        x += v * dt * 0.5f;
      }
    }
  };
  struct Leg {
    glm::vec3 foot, from;
    bool swinging = false;
    float t = 0.0f, duration = 0.1f;
  };

  std::vector<glm::vec3> bind, points;
  std::vector<RigBone> bones;
  SpiderRig rig;
  SpiderParams params;
  std::vector<Leg> legs;
  bool placed = false;
  glm::vec3 position = glm::vec3(0.0f), velocity = glm::vec3(0.0f);
  glm::mat3 axes = glm::mat3(1.0f);
  FloorQuery floor;
  Spring bob, pitch, roll, head; // the torso's and the head's shake
  glm::vec2 torsoShift = glm::vec2(0.0f); // where the middle of the torso is (x, z), in the body's frame
  bool looking = false;
  glm::vec3 lookTarget = glm::vec3(0.0f);
  float twist = 0.0f;          // how far the head is twisted on its neck now (radians, may pass half a turn)
  glm::quat tilt = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // and how much it is nodded
  glm::mat4 neckTurn = glm::mat4(1.0f), headTurn = glm::mat4(1.0f); // what the look adds to the bones

  float groundAt(const glm::vec3 &world) const;
  glm::vec3 restPlace(const SpiderLeg &leg) const;
  void landed(size_t leg, float speed);
  void lifted(size_t leg, float speed);
  void poseBody();
  void aimFace(float dt);
};

#endif
