#ifndef NET_SERVER
#define NET_SERVER

#include <memory>
#include <string>
#include <vector>

#include "Connection.h"
#include "GameStage.h"
#include "Protocol.h"

// The server's side of the network: it lets clients in (each one becomes a Player of the stage),
// takes what they send (their controls, their actions) and passes it to the stage, and tells
// every client what the world looks like (a snapshot of every dynamic object, thirty times a
// second). It knows nothing of how the world is simulated: that is the stage's, which main steps.
class NetServer {
private:
  struct Client {
    std::unique_ptr<NetConnection> connection;
    int playerId = -1; // -1 until its Hello is accepted
    std::string name;
    double connectedAt = 0.0;
  };

  GameStage *stage;
  int mapIndex;
  std::string mapName;
  uint32_t dynamicCount; // the objects the map made (before any player came): a client's map must have as many
  NetListener listener;
  std::vector<std::unique_ptr<Client>> clients;
  int nextPlayerId = 1;
  double clock = 0.0; // seconds the server has run
  uint32_t snapshotCount = 0;
  NetWriter objects; // the objects of the snapshot being sent (the same for everybody)
  uint16_t objectCount = 0;
  bool reset = false;

  void handle(Client &client, const NetConnection::Message &message);
  void hello(Client &client, NetReader &in);
  void drop(Client &client, const std::string &why);
  void send(Client &client, uint8_t type, const NetWriter &payload) { client.connection->send(type, payload); }
  void broadcast(uint8_t type, const NetWriter &payload, const Client *except = nullptr);
  void relocate(NetReader &in);
  void changeProperty(NetReader &in);
  void command(Player &player, const std::string &text);
  void sendSpawn(Client *to, const Stage::NetEvent &event);
  void notice(Player &player, const std::string &text);
  Client *clientOf(int playerId);

public:
  NetServer(GameStage &stage, int mapIndex, const std::string &mapName)
      : stage(&stage), mapIndex(mapIndex), mapName(mapName),
        dynamicCount((uint32_t)stage.getDynamicObjects().size()) {
    stage.trackNet(); // (what appears from now on is told to the clients)
  }

  // The /reset command asked for the map to start again
  bool resetWanted() const { return reset; }
  void cancelReset() { reset = false; }
  // Tells the clients and lets them go, then serves `next` (a new map, the same one) instead
  void restart(GameStage &next);

  bool start(int port, std::string &error);
  // Accepts new clients and takes what the ones there are have sent; drops the ones that went
  void poll(double dt);
  // Tells every client how the world is now
  void sendSnapshots();
  size_t playerCount() const;
};

#endif
