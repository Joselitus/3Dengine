#ifndef NET_CLIENT
#define NET_CLIENT

#include <deque>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Connection.h"
#include "GameStage.h"
#include "Protocol.h"

// The client's side of the network. It connects to a server, tells it what the player does (his
// controls, his actions), and keeps the client's copy of the world as the server says it is:
//
//  - every dynamic object of the stage (which the client built from the same map as the server)
//    is a replica, moved between the snapshots the server sends (kept INTERPOLATION_DELAY
//    behind, so there are always two to move between) and told the rest of its state when its
//    snapshot's time comes (GameObject::readNetState);
//  - the player's own penguin is the exception: it moves at once with his controls on the
//    client too (prediction: pressing a key must not wait for the server) and is pulled
//    smoothly towards where the server had it when it received that input (reconciliation);
//    while he drives a vehicle or is dead, it is a replica like the rest;
//  - the other players are Walkers made when the server says they joined.
//
// Using it: connect() (a new object), update() every frame until isReady() (the server accepted
// and said which map), make that map and attach() it, then every frame send the controls and call
// apply(). isFailed() says it all went wrong (getError() why).
class NetClient {
public:
  enum class State { Connecting, Greeting, Ready, Playing, Failed };

private:
  struct Peer {
    int id;
    std::string name;
    int bodyNetId;
  };
  struct ObjectRecord {
    int netId;
    uint8_t flags;
    glm::vec3 position;
    glm::quat turn;
    float scale;
    glm::vec3 velocity;
    std::vector<uint8_t> extra; // its own state (readNetState)
  };
  struct Snapshot {
    double time = 0.0; // server seconds
    float timeOfDay = 0.0f, dayDuration = 0.0f, timeScale = 1.0f;
    unsigned ackSeq = 0;
    std::vector<ObjectRecord> objects;
    std::unordered_map<int, size_t> index; // netId -> objects
    bool extrasApplied = false;
  };
  struct Step {
    unsigned seq;
    glm::vec3 position;
  };

  std::unique_ptr<NetConnection> connection;
  State state = State::Connecting;
  std::string error;
  std::string playerName;
  std::vector<std::string> notices;

  // From the server's welcome
  int myId = -1;
  int mapIndex = 0;
  std::string mapName;
  uint32_t dynamicCount = 0;
  std::vector<Peer> peers;
  std::set<std::string> warnedClasses; // (the classes whose network state did not match, said once)
  std::vector<std::pair<bool, std::vector<uint8_t>>> pendingEvents; // spawns, before the map is made
  void handleSpawn(bool spawn, NetReader &in);

  GameStage *stage = nullptr;
  std::deque<Snapshot> snapshots;
  double serverTime = 0.0;
  uint32_t lastMilliseconds = 0;
  bool haveServerTime = false;
  double renderTime = 0.0;
  bool haveRenderTime = false;

  // Sending controls
  unsigned inputSeq = 0;
  double sinceInput = 1.0;

  // The player's own penguin
  std::deque<Step> history;
  glm::vec3 correction = glm::vec3(0.0f);
  bool wasPredicted = false;

  void handle(const NetConnection::Message &message);
  void readWelcome(NetReader &in);
  void readSnapshot(NetReader &in);
  void reconcile(const Snapshot &snapshot);
  void fail(const std::string &why);
  ObjectRecord *recordOf(Snapshot &s, int netId);

public:
  // Starts connecting to host (a name or an address; "host:port" gives the port, else the usual
  // one). It does not wait: call update().
  NetClient(const std::string &address, const std::string &name);

  State getState() const { return state; }
  bool isReady() const { return state == State::Ready; }
  bool isFailed() const { return state == State::Failed; }
  // Connecting or greeting
  bool isConnecting() const { return state == State::Connecting || state == State::Greeting; }
  const std::string &getError() const { return error; }
  // What the server says about the map to make
  int getMapIndex() const { return mapIndex; }
  const std::string &getMapName() const { return mapName; }
  int getPlayerId() const { return myId; }
  // The messages the server wanted the player to read, since the last call
  std::vector<std::string> takeNotices() {
    std::vector<std::string> taken;
    taken.swap(notices);
    return taken;
  }
  // Sends and receives (every frame)
  void update();
  // The map is made: its players are put in it, the ones the server said (the player's own
  // among them). False (getError) if it is not the server's map
  bool attach(GameStage &stage);

  // The player's controls (rate-limited: it sends at most INPUT_INTERVAL apart; the number
  // of the one sent, or 0 if it kept this one). `dt` since the last call.
  unsigned sendInput(double dt, const glm::vec2 &move, float up, float yaw, float pitch, bool running);
  // The player uses the interactable number `index` of the map, standing at `where`
  void sendUse(unsigned interactableIndex, const glm::vec3 &where);
  void sendAction(uint8_t action);
  void sendFire(const glm::vec3 &eye, const glm::vec3 &direction);
  void sendCommand(const std::string &text);
  void disconnect();

  // Where the player's own penguin is after this frame's physics, for input number `seq`
  void recordPrediction(unsigned seq, const glm::vec3 &position);
  // Moves the replicas on to what they look like now (every frame, before the stage updates)
  void apply(double dt);
  // How far behind the newest snapshot the world is shown, and the round trip, for the debug line
  double getRenderLag() const { return haveRenderTime && !snapshots.empty() ? snapshots.back().time - renderTime : 0.0; }
};

#endif
