#ifndef GNOME
#define GNOME

#include <functional>
#include <memory>
#include <vector>

#include "DynamicGameObject.h"

// The garden gnome (assets/gnome, generate_gnome.py): a 0.84 m tall figure that stands where it was
// put, perfectly still, with no animation at all, like the ones in a garden. It is born with a
// beaming face. Every time a player LOOKS AT HIS FACE (from in front of him, within LOOK_RANGE
// metres and a cone of LOOK_CONE degrees round the line of sight, for LOOK_HOLD seconds) his face
// is angrier the next time it is seen: the change happens once nobody is looking at him any more.
// The five faces (gnome_face_0..4): 0 beaming, 1 smile fading, 2 annoyed, 3 furious, 4 a wide smile
// again, with white glowing eyes. The fifth look (at face 4, held for FINAL_HOLD seconds) is the
// one that sets him off: a knife appears in his hand and he runs at the nearest player who is on
// foot and alive (CHASE_SPEED: a walking penguin is slower, a running one is faster) and kills
// whoever he catches (the stage's callback). Players in a vehicle or a safe place are not his
// business: he stands where he is, knife in hand, until one comes out. Two shots (takeDamage) kill
// him, for good.
//
// Nothing about his looks is in the clients' hands: the server decides who looks at him (the
// stage tells him where each player's eyes are and where they look: setViewersQuery, and whether
// the ground is in the way: setClearViewQuery) and what face
// he has, and sends it (writeNetState). A replica only shows the face, the knife and the legs'
// swing as he runs.
class Gnome : public DynamicGameObject {
public:
  // A player's eyes and line of sight
  struct Viewer {
    glm::vec3 eye = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f); // unit
  };
  // The player he runs at
  struct Victim {
    int id = -1;
    glm::vec3 position = glm::vec3(0.0f); // his feet
  };

  static constexpr int FACES = 5;               // 0 .. LAST_FACE
  static constexpr int LAST_FACE = FACES - 1;
  static constexpr float LOOK_RANGE = 45.0f;    // m
  static constexpr float LOOK_CONE = 13.0f;     // degrees
  static constexpr float LOOK_HOLD = 0.3f;      // s looked at to count as a look
  static constexpr float FINAL_HOLD = 0.9f;     // s the last face is looked at before he is armed
  static constexpr float AWAY_DELAY = 0.2f;     // s without anybody looking before the face changes
  static constexpr float FACE_HEIGHT = 0.57f;   // m: where the face is (gnome_body's head)
  static constexpr float CHASE_SPEED = 7.5f;    // m/s (a penguin walks at 4 and runs at 7: he catches even a runner)
  static constexpr float CHASE_RANGE = 90.0f;   // m: he gives up on those farther away
  static constexpr float KILL_DISTANCE = 0.8f;  // m
  static constexpr float STRIDE = 0.55f;        // m per step cycle
  static constexpr float TURN_RATE = 9.0f;      // rad/s
  static constexpr int HEALTH = 2;              // shots

private:
  int face = 0;
  bool armed = false;
  bool dead = false;
  bool pendingChange = false; // he has been looked at: the face changes when nobody does
  float lookTime = 0.0f, awayTime = 0.0f;
  float walkPhase = 0.0f, walkAmount = 0.0f, raise = 0.0f;
  int health = HEALTH;
  glm::vec3 lastPosition = glm::vec3(0.0f);

  std::function<void(std::vector<Viewer> &)> viewers;
  std::function<bool(const glm::vec3 &, Victim &)> findVictim;
  std::function<bool(const glm::vec3 &, const glm::vec3 &)> clearView;
  std::function<void(int)> caught;

  size_t facePart[FACES] = {0, 0, 0, 0, 0};
  size_t eyesPart = 0, armPart = 0, knifePart = 0, legPart[2] = {0, 0};

  bool isLookedAt() const;
  void arm();
  void animate(double dt);
  void showFace();
  void updateReplica(double dt);

public:
  // `faces`: gnome_face_0..4; `eyes`: the glowing white eyes of face 4
  Gnome(std::shared_ptr<Model> body, const std::vector<std::shared_ptr<Model>> &faces,
        std::shared_ptr<Model> eyes, std::shared_ptr<Model> arm, std::shared_ptr<Model> knife,
        std::shared_ptr<Model> legLeft, std::shared_ptr<Model> legRight);

  // Where the players' eyes are and where they look (the ones who can see: alive, on foot)
  void setViewersQuery(std::function<void(std::vector<Viewer> &)> q) { viewers = q; }
  // Whether nothing (the ground) is between two points: a look only counts through it
  void setClearViewQuery(std::function<bool(const glm::vec3 &, const glm::vec3 &)> q) { clearView = q; }
  // Whom he runs at: the nearest player alive and out in the open, from his position; false if none
  void setVictimQuery(std::function<bool(const glm::vec3 &, Victim &)> q) { findVictim = q; }
  // He reaches a player (the stage kills him; the argument is his id)
  void setCaughtCallback(std::function<void(int)> c) { caught = c; }

  int getFace() const { return face; }
  bool isArmed() const { return armed; }
  bool isDead() const { return dead; }

  void update(double dt) override;
  void takeDamage(float amount, const glm::vec3 &direction, const Stage &stage) override;
  void teleport(const glm::vec3 &position) override;
  void writeNetState(NetWriter &out) const override;
  void readNetState(NetReader &in) override;
  void describe(std::vector<std::string> &lines) const override;
};

#endif
