#ifndef GAME_STAGE
#define GAME_STAGE

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "EntityContext.h"
#include "Interactable.h"
#include "MirrorView.h"
#include "SpotLight.h"
#include "PlayableCharacter.h"
#include "Stage.h"
#include "Walker.h"

// Light and colours of a map. The light is a far point light in `lightDir`
// (the sun or the moon); `horizon` is the clear colour and the fog colour.
struct Environment {
  glm::vec3 lightDir = glm::normalize(glm::vec3(-0.3f, 0.8f, -0.5f));
  glm::vec3 lightColor = glm::vec3(1.0f);
  glm::vec3 horizon = glm::vec3(0.5f, 0.7f, 0.9f);
  // For a procedural sky (setSky with unlit 3): colour straight up, direction
  // to the sun, and how visible the stars are (0..1)
  glm::vec3 skyZenith = glm::vec3(0.2f, 0.4f, 0.8f);
  glm::vec3 sunDir = glm::vec3(0.0f, 1.0f, 0.0f);
  float starAlpha = 0.0f;
  // The procedural sky paints dunes along the horizon (the desert); off for other landscapes
  bool skyDunes = true;
  // A canopy: an invisible mask of leaves above the map that shades the light of the sun (or of
  // whatever lights the map from far away). Each point is lit as much as the mask lets through
  // where the ray from it towards the light crosses the canopy's planes, at the heights
  // canopyHeights (one per channel of the texture, R G B: so a tall crown casts a long shadow).
  // The texture (GL; 0 = no canopy) covers the square from canopyMin, canopySize metres a side;
  // 255 = open, 0 = leaves. canopyStrength: how dark the leaves make it (0..1).
  unsigned int canopyMask = 0;
  glm::vec2 canopyMin = glm::vec2(0.0f);
  float canopySize = 1.0f;
  glm::vec3 canopyHeights = glm::vec3(0.0f);
  float canopyStrength = 0.85f;
  // 1: that sky has a line of trees on the horizon instead of dunes
  float forestHorizon = 0.0f;
};

// One person playing. The server has one for every client that is connected; a client has only
// its own (GameStage::getLocalPlayer), filled in from what the server tells it.
//
// A player always has a body on foot (`walker`, a penguin) and `character` is what his controls
// move now: the walker, or the vehicle he drives (the RV, Bob's ship).
struct Player {
  int id = 0;
  std::string name;
  std::shared_ptr<Walker> walker;
  std::shared_ptr<PlayableCharacter> character;
  // How the camera follows `character` (distance 0 = first person), and where it starts looking.
  // `controlSerial` changes every time they do (the client attaches its controller again)
  float cameraDistance = 0.0f, cameraHeight = 1.6f, cameraYaw = 0.0f;
  unsigned controlSerial = 0;
  bool dead = false;
  bool abducted = false; // the way he died: Bob took him
  glm::vec3 abductPoint = glm::vec3(0.0f);
  double timeDead = 0.0; // seconds since he died
  // On a map with vehicles (VehicleStage)
  bool inVehicle = false; // he drives the RV
  bool seated = false;    // he sits in the RV's passenger seat
  bool inSaucer = false;  // he flies Bob's ship
  float paralysis = 0.0f; // seconds left paralysed by Bob's ray
  // The Flatwoods monster holds him: his body walks out of the RV by itself (possessStep: to the
  // doorway, out through it, away from it, then it stands) until he presses the leave key
  bool possessed = false;
  int possessStep = 0;
  float possessStepTime = 0.0f;
  float possessYaw = 0.0f; // where his body walks (his camera turns to it)
  float savedWalkSpeed = 0.0f;
  // What his controls say now (the server keeps the last from his client)
  glm::vec2 moveDir = glm::vec2(0.0f);
  float moveUp = 0.0f;
  float lookYaw = 0.0f, lookPitch = 0.0f;
  bool running = false;
  unsigned inputSeq = 0; // the last input received
  // The server: a message to show him, once (taken by the network code)
  std::vector<std::string> notices;

  glm::vec3 getPosition() const { return character ? character->getPosition() : walker->getPosition(); }
};

// A playable map: a Stage that also knows everything the game needs to run
// it, so maps can be swapped at runtime (see MapSelector): its environment,
// an optional sky dome, the player and the camera it wants, and the objects
// the player can use. Abstract like Stage: each map fills these in its
// constructor (see TestStage in test.cpp, SceneStage).
class GameStage : public Stage {
protected:
  Environment environment;
  std::shared_ptr<GameObject> sky; // drawn around the camera, behind all
  float farPlane = 300.0f;     // how far the camera sees (a map with far scenery raises it)
  std::vector<Interactable *> interactables; // owned by the stage
  glm::vec3 viewer = glm::vec3(0.0f); // where the camera is (things may look at it)
  glm::mat4 viewProjection = glm::mat4(1.0f); // and what it sees (world -> clip space)
  unsigned seenControlSerial = 0; // see takePlayerChange

  // The people playing (see Player). The server adds one for each client; a client adds its own
  // and the others' (to draw them).
  std::vector<std::unique_ptr<Player>> players;
  Player *local = nullptr; // a client: the one at this computer (null on the server)
  Player *acting = nullptr; // the player whose action is being carried out (see performAs)
  // Where new players appear (their feet), set by the map's constructor
  glm::vec3 spawnPoint = glm::vec3(0.0f);
  float spawnYaw = 0.0f;
  // How the camera follows a player on foot (distance 0 = first person, from his eyes)
  float walkCameraDistance = 0.0f, walkCameraHeight = 1.6f;
  std::string avatarModel = "../assets/ping/PenguinoAnimado.fbx";
  static constexpr unsigned int AVATAR_ANIMATION = 1; // (the clean take of the dance, see VehicleStage)
  static constexpr float AVATAR_GRAVITY = 25.0f;
  // How long a dead player waits (s) before he starts again at the spawn point
  static constexpr double RESPAWN_DELAY = 14.0;

  explicit GameStage(FloorMode mode) : Stage(mode) {}

  // A client: asks the server to fill the RV's tank (a pump's button)
  std::function<void()> refuelRequest;

  // Dynamic objects stay on the floor (override for other rules)
  void apply(DynamicGameObject &object, double dt) override {
    collideWithFloor(object, dt);
  }

  // Hands the player's controls (and, on his client, the camera) to another character (e.g. when
  // he gets into a vehicle): his client notices it (controlSerial) and attaches its controller
  // to the new character with this distance (0 = first person) and height
  void setControl(Player &p, std::shared_ptr<PlayableCharacter> character, float distance,
                  float height, float yaw = 0.0f) {
    p.character = character;
    p.cameraDistance = distance;
    p.cameraHeight = height;
    p.cameraYaw = yaw;
    p.controlSerial++;
  }

  // A sky dome model (drawn unlit; see shader.frag). unlit 1 = textured with
  // twinkling stars, 3 = painted by the shader from the Environment (sun,
  // stars, colours), so it can follow the time of day
  void setSky(std::shared_ptr<Model> model, int unlit = 1);
  // Ground height at (x, z), or `fallback` where there is no floor
  float groundAt(float x, float z, float fallback) const;

  // Where the player's body stands when it appears (index: how many bodies appeared before, so
  // that they do not all stand on the same spot)
  virtual glm::vec3 spawnPosition(int index) const;
  // The player is back from the dead, at the spawn point (a map with more state to reset
  // overrides this and calls it)
  virtual void respawn(Player &p);
  // The player died or was taken: a map with vehicles lets go of what he was driving
  virtual void onPlayerGone(Player &p) {}
  // He may use things now / he can't move or look (paralysed, held...)
  virtual bool canInteract(const Player &p) const { return !p.dead; }
  // Called by tick() before the objects are updated
  virtual void onTick(double dt) {}
  virtual bool isImmobilized(const Player &p) const { return false; }

public:
  // --- The people playing
  // A new player with his body on foot at the spawn point (appearing in the stage). `netId`: the
  // number the body has for the network (-1: the next one), as the server gave it
  Player &addPlayer(int id, const std::string &name, int netId = -1);
  void removePlayer(int id);
  // A client: every dynamic object is a copy of the server's (GameObject::setReplica): it moves
  // as the server says, and does not think by itself
  void makeReplicas();
  // A client: the server says something appeared (see Stage::trackNet): makes the same, with that
  // network number, as a replica. False if this map does not know such a thing.
  // Map editing (see MapEdits): the kinds of creature or NPC this map can have more of, and one made
  // at `where` facing `yaw` (false if the map does not know the kind)
  virtual std::vector<std::string> entityKinds() const { return {}; }
  virtual bool spawnEntity(const std::string &kind, const glm::vec3 &where, float yaw, EntityContext &context) {
    return false;
  }
  virtual bool spawnReplica(unsigned char kind, int netId, const glm::vec3 &where, float arg) { return false; }
  Player *findPlayer(int id);
  const std::vector<std::unique_ptr<Player>> &getPlayers() const { return players; }
  // A client: the player at this computer (null on the server)
  void setLocalPlayer(Player *player) { local = player; }
  void setRefuelRequest(std::function<void()> request) { refuelRequest = request; }
  Player *getLocalPlayer() const { return local; }

  // The player dies (a creature caught him): his controls stop and nothing can be used. His client
  // shows it (the camera falls and looks up, the screen goes red); a few seconds later (RESPAWN_DELAY)
  // he is back at the spawn point.
  void killPlayer(Player &p);
  // The player is taken (Bob caught him): the same end as dying, but his client shows it
  // differently: he rises in the beam of light towards `into` (the ship's hatch) while the
  // screen goes white, then black
  void abductPlayer(Player &p, const glm::vec3 &into);
  // The server: moves time on (the clock, the objects) and the players' own timers
  void tick(double dt);
  // The server: what a client's controls say now, for his player to obey (his character gets it
  // every tick, see tick)
  void setInput(Player &p, const glm::vec2 &move, float up, float yaw, float pitch, bool running,
                unsigned seq);

  // Carries out `action` for player `p`: the handlers below that do not get the player in their
  // arguments (an interactable's onUse...) find him in `acting`
  template <typename F> void performAs(Player &p, F action) {
    Player *before = acting;
    acting = &p;
    action();
    acting = before;
  }

  // --- What the server does when a player acts (the client asks, see net/)
  // The "leave the vehicle" key: a map where the player can drive something gives the controls
  // back to a character on foot
  virtual void leaveVehicle(Player &p) {}
  // The headlights key: the player's flashlight goes on or off (a map with a vehicle the player
  // is driving turns its lights instead)
  virtual void toggleHeadlights(Player &p) { p.walker->toggleFlashlight(); }
  // The camera key: a map with a vehicle the player is driving changes the point of view
  // (inside it / from behind)
  virtual void toggleVehicleCamera(Player &p) {}
  // The ship-legs key (Q): flying Bob's ship, its landing legs go in or out
  virtual void toggleShipLegs(Player &p) {}
  // Debug: bursts a random tyre of the vehicle (a map with one)
  virtual void popRandomTire(Player &p) {}
  // The fire button: the player's eye at `eye` looking along `direction`: a map where he can
  // shoot (Bob's ship's ray gun) shoots
  virtual void fire(Player &p, const glm::vec3 &eye, const glm::vec3 &direction) {}
  // The engine key: a map with a vehicle the player is driving switches its engine on or off
  virtual void toggleEngine(Player &p) {}
  // The handbrake key: a map with a vehicle the player is driving pulls or releases its handbrake
  virtual void toggleHandbrake(Player &p) {}
  // The player used a thing that acts at once (Interactable::usesDirectly): it does it, if he
  // was in reach of it where his client says he was (`at`: his position there; it has to be near
  // where the server has him)
  virtual void useInteractable(Player &p, Interactable &target, const glm::vec3 &at);
  // The player fills the tank at a pump (a panel's button): a map with a vehicle fills its
  virtual void refuel(Player &p) {}

  // --- What the client shows (about its own player)
  bool isPlayerDead() const { return local && local->dead; }
  bool isPlayerAbducted() const { return local && local->abducted; }
  const glm::vec3 &getAbductPoint() const {
    static const glm::vec3 none(0.0f);
    return local ? local->abductPoint : none;
  }
  // The player can't move or look (paralysed by Bob's ray, held by Bob): the main loop stops the
  // controller (nothing by default)
  virtual bool playerImmobilized() const { return local && isImmobilized(*local); }
  // How paralysed he is (0 = not .. 1 = just hit), for the yellow tint of the screen
  virtual float playerParalysis() const { return 0.0f; }
  // Bob holds him: how near he is to getting free (0..1), or < 0 if nobody holds him
  virtual float struggleProgress() const { return -1.0f; }
  // How much the player feels Bob near (0..1): the main loop covers the screen with that much film
  // grain (nothing by default)
  virtual float alienPresence() const { return 0.0f; }
  // The abduction is over (he is in the ship): Bob's hiss stops, though the grain stays
  virtual void endAlienHiss() {}
  // The main loop tells the map where the camera is and what it sees, every frame
  void setViewer(const glm::vec3 &position, const glm::mat4 &projection) {
    viewer = position;
    viewProjection = projection;
  }
  const Environment &getEnvironment() const { return environment; }

  // True once after the local player's controls were handed to another character (a vehicle...):
  // then the controller has to be attached to getPlayer() again
  bool takePlayerChange() {
    if (!local || local->controlSerial == seenControlSerial)
      return false;
    seenControlSerial = local->controlSerial;
    return true;
  }
  // The player aims a gun (the main loop shows a crosshair)
  virtual bool playerAiming() const { return false; }
  // Something controls the player's body (the Flatwoods monster): the main loop shows which key
  // frees him, and turns the camera to `yaw` (where his body walks) when possessedLook says so
  bool playerPossessed() const { return local && local->possessed && !local->dead; }
  bool possessedLook(float &yaw) const {
    yaw = local ? local->possessYaw : 0.0f;
    return local && local->possessed && !local->inVehicle;
  }
  // The main loop is about to draw a mirror's picture (true), or the player's view (false): what
  // only shows in mirrors (the Flatwoods monster) shows or hides
  virtual void setMirrorView(bool inMirror) {}
  // Rear-view mirrors (`side` 0, 1: see RV; as many as the map has) that show what is behind, drawn by the main loop from a camera of its
  // own into a texture (see RV): true, and where that camera is, if there is one to draw now
  virtual bool rearMirror(int side, MirrorView &view) const { return false; }
  // The GL texture (2D, RGBA) the mirror's glass shows, and its aspect (width / height); given
  // once, when the map is made
  virtual void setRearMirrorTexture(int side, unsigned int texture, float aspect) {}
  // The glass hides itself while the picture it shows is being drawn (it would draw itself)
  virtual void showRearMirror(int side, bool show) {}
  // Adds the spot lights that are on right now (the shader takes the first
  // few; see the main loop)
  virtual void getSpotLights(std::vector<SpotLight> &lights) const;
  // False while the player can't use objects (e.g. while driving)
  virtual bool interactionsEnabled() const { return local && canInteract(*local); }
  std::shared_ptr<PlayableCharacter> getPlayer() const {
    return local ? local->character : nullptr;
  }
  float getCameraDistance() const { return local ? local->cameraDistance : 0.0f; }
  float getCameraHeight() const { return local ? local->cameraHeight : 1.6f; }
  float getFarPlane() const { return farPlane; }
  // Where the players start (the map editor begins its look there)
  glm::vec3 getSpawnPoint() const { return spawnPoint; }
  float getCameraYaw() const { return local ? local->cameraYaw : 0.0f; }
  const std::vector<Interactable *> &getInteractables() const {
    return interactables;
  }

  // The player (the penguin or the vehicle) is always drawn, wherever it is
  bool edgeCullExempt(const GameObject &object) const override;

  // Draws the sky around the camera (without depth, so it stays behind
  // everything) and then the stage
  void render(Shader *shader, const glm::vec3 &cameraPosition, double time);
};

#endif
