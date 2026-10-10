#include "NetServer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "MapList.h"
#include "Protocol.h"

using namespace glm;
using namespace Net;

namespace {
// The turn of an object as a quaternion (its rotation matrix may carry a scale)
quat turnOf(const mat4 &m) {
  mat3 r(m);
  for (int i = 0; i < 3; i++) {
    float l = length(r[i]);
    if (l > 1e-6f)
      r[i] /= l;
  }
  return normalize(quat_cast(r));
}
} // namespace

bool NetServer::start(int port, std::string &error) { return listener.listen(port, error); }

size_t NetServer::playerCount() const {
  size_t n = 0;
  for (const auto &c : clients)
    if (c->playerId >= 0)
      n++;
  return n;
}

NetServer::Client *NetServer::clientOf(int playerId) {
  for (auto &c : clients)
    if (c->playerId == playerId)
      return c.get();
  return nullptr;
}

void NetServer::broadcast(uint8_t type, const NetWriter &payload, const Client *except) {
  for (auto &c : clients)
    if (c->playerId >= 0 && c.get() != except)
      send(*c, type, payload);
}

void NetServer::poll(double dt) {
  clock += dt;
  while (auto connection = listener.accept()) {
    printf("[net] connection from %s\n", connection->peerName().c_str());
    std::unique_ptr<Client> client(new Client());
    client->connection = std::move(connection);
    client->connectedAt = clock;
    clients.push_back(std::move(client));
  }
  for (auto &client : clients) {
    client->connection->update();
    NetConnection::Message message;
    while (client->connection->isOpen() && client->connection->next(message))
      handle(*client, message);
    // A client that never says hello is a stranger (or a port scan)
    if (client->playerId < 0 && clock - client->connectedAt > 10.0)
      drop(*client, "never said hello");
  }
  // The ones that have gone
  for (size_t i = 0; i < clients.size();) {
    Client &c = *clients[i];
    if (!c.connection->isOpen()) {
      if (c.playerId >= 0) {
        int id = c.playerId;
        printf("[net] %s (player %d) has left: %s\n", c.name.c_str(), id, c.connection->getError().c_str());
        stage->removePlayer(id);
        NetWriter w;
        w.i32(id);
        c.playerId = -1;
        clients.erase(clients.begin() + i);
        broadcast(S_LEFT, w);
        continue;
      }
      clients.erase(clients.begin() + i);
      continue;
    }
    i++;
  }
}

void NetServer::drop(Client &client, const std::string &why) {
  printf("[net] dropping %s: %s\n", client.name.empty() ? client.connection->peerName().c_str() : client.name.c_str(),
         why.c_str());
  client.connection->close();
}

void NetServer::hello(Client &client, NetReader &in) {
  uint32_t version = in.u32();
  std::string name = in.string();
  if (!in.isOk() || client.playerId >= 0) {
    drop(client, "invalid hello");
    return;
  }
  auto reject = [&](const std::string &reason) {
    NetWriter w;
    w.string(reason);
    send(client, S_REJECT, w);
    client.connection->update(); // (let the reason out before the door closes)
    drop(client, reason);
  };
  if (version != PROTOCOL_VERSION) {
    reject("Version del protocolo distinta: el servidor usa la " + std::to_string(PROTOCOL_VERSION));
    return;
  }
  if ((int)playerCount() >= MAX_PLAYERS) {
    reject("El servidor esta lleno");
    return;
  }
  if (name.empty())
    name = "Pingu";
  if (name.size() > 24)
    name.resize(24);
  Player &player = stage->addPlayer(nextPlayerId++, name);
  client.playerId = player.id;
  client.name = name;
  printf("[net] %s joins as player %d (%zu in total)\n", name.c_str(), player.id, playerCount());

  NetWriter w;
  w.u32(PROTOCOL_VERSION);
  w.i32(player.id);
  w.u16((uint16_t)mapIndex);
  w.string(mapName);
  w.u32(dynamicCount);
  w.u16((uint16_t)stage->getPlayers().size());
  for (const auto &p : stage->getPlayers()) {
    w.i32(p->id);
    w.string(p->name);
    w.i32(p->walker->getNetId());
  }
  send(client, S_WELCOME, w);
  // What has appeared since the map was made (the clients do not have it)
  for (const auto &entry : stage->getLiveSpawns())
    sendSpawn(&client, entry.second);

  NetWriter joined;
  joined.i32(player.id);
  joined.string(name);
  joined.i32(player.walker->getNetId());
  broadcast(S_JOINED, joined, &client);
}

// Tells `to` (everybody if null) that something appeared or went away
void NetServer::sendSpawn(Client *to, const Stage::NetEvent &event) {
  NetWriter w;
  w.i32(event.netId);
  if (event.spawn) {
    w.u8(event.kind);
    w.vec3(event.where);
    w.f32(event.arg);
  }
  if (to)
    send(*to, event.spawn ? S_SPAWN : S_DESPAWN, w);
  else
    broadcast(event.spawn ? S_SPAWN : S_DESPAWN, w);
}

void NetServer::notice(Player &player, const std::string &text) {
  if (Client *c = clientOf(player.id)) {
    NetWriter w;
    w.string(text);
    send(*c, S_NOTICE, w);
  }
}

// The few commands a player can give from the console
void NetServer::command(Player &player, const std::string &text) {
  std::string word = text;
  if (!word.empty() && word[0] == '/')
    word.erase(0, 1);
  std::string argument;
  size_t space = word.find(' ');
  if (space != std::string::npos) {
    argument = word.substr(space + 1);
    word.resize(space);
  }
  if (word == "day") {
    stage->setTimeOfDay(12.0f);
    notice(player, "Hora: 12:00");
  } else if (word == "night") {
    stage->setTimeOfDay(0.0f);
    notice(player, "Hora: 00:00");
  } else if (word == "time" && !argument.empty()) {
    stage->setTimeOfDay((float)atof(argument.c_str()));
    notice(player, "Hora: " + argument);
  } else if (word == "reset") {
    printf("[net] %s asks for the map to start again\n", player.name.c_str());
    reset = true;
  } else if (word == "map") {
    const std::vector<MapEntry> &maps = mapList();
    int index = -1;
    for (size_t i = 0; i < maps.size(); i++)
      if (argument == maps[i].name || argument == std::to_string(i))
        index = (int)i;
    if (index < 0) {
      std::string names;
      for (size_t i = 0; i < maps.size(); i++)
        names += (i ? ", " : "") + std::to_string(i) + " " + maps[i].name;
      notice(player, "No hay ese mapa. Mapas: " + names);
      return;
    }
    printf("[net] %s asks for map '%s'\n", player.name.c_str(), maps[index].name.c_str());
    nextMap = index;
    reset = true;
  } else {
    notice(player, "Comando desconocido en el servidor: " + text);
  }
}

// Debug: a client moved or turned an object
void NetServer::relocate(NetReader &in) {
  uint8_t kind = in.u8();
  int id = in.i32();
  uint8_t flags = in.u8();
  vec3 position = in.vec3();
  float heading = in.f32();
  if (!in.isOk())
    return;
  GameObject *object = nullptr;
  if (kind == 0) {
    const auto &list = stage->getObjects();
    if (id >= 0 && id < (int)list.size())
      object = list[id].get();
  } else {
    object = stage->findDynamic(id);
  }
  if (!object)
    return;
  if (flags & 1)
    stage->relocate(*object, position);
  if (flags & 2)
    stage->turn(*object, std::remainder(heading - object->getHeading(), 2.0f * pi<float>()));
  // The static ones do not travel in the snapshots: everybody is told
  if (kind == 0) {
    NetWriter w;
    w.u8(kind);
    w.i32(id);
    w.u8(flags);
    w.vec3(object->getPosition());
    w.f32(object->getHeading());
    broadcast(S_RELOCATED, w);
  }
}

// Debug: a client changed a value in the properties window
void NetServer::changeProperty(NetReader &in) {
  uint8_t kind = in.u8();
  int id = in.i32();
  std::string name = in.string();
  float value = in.f32();
  if (!in.isOk())
    return;
  std::vector<Property> properties;
  if (kind == 2) {
    stage->getProperties(properties);
  } else {
    GameObject *object = nullptr;
    if (kind == 0) {
      const auto &list = stage->getObjects();
      if (id >= 0 && id < (int)list.size())
        object = list[id].get();
    } else {
      object = stage->findDynamic(id);
    }
    if (!object)
      return;
    object->getProperties(properties);
  }
  for (Property &p : properties) {
    if (p.name != name)
      continue;
    if (p.kind == Property::Kind::Action) {
      if (p.run)
        p.run();
    } else if (p.set) {
      p.set(value);
    }
    break;
  }
}

void NetServer::restart(GameStage &next, int index, const std::string &name) {
  NetWriter w;
  for (auto &c : clients) {
    if (c->playerId >= 0)
      send(*c, S_RESET, w);
    c->connection->update(); // (let it out before the door closes)
    c->connection->close();
  }
  clients.clear();
  stage = &next;
  mapIndex = index;
  mapName = name;
  nextMap = -1;
  dynamicCount = (uint32_t)next.getDynamicObjects().size();
  next.trackNet();
  reset = false;
  printf("[net] map '%s' started\n", mapName.c_str());
}

void NetServer::handle(Client &client, const NetConnection::Message &message) {
  NetReader in(message.payload);
  if (client.playerId < 0) {
    if (message.type == C_HELLO)
      hello(client, in);
    else
      drop(client, "message before the hello");
    return;
  }
  Player *player = stage->findPlayer(client.playerId);
  if (!player)
    return;
  switch (message.type) {
  case C_INPUT: {
    unsigned seq = in.u32();
    float mx = in.f32(), my = in.f32(), up = in.f32(), yaw = in.f32(), pitch = in.f32();
    uint8_t flags = in.u8();
    if (in.isOk() && (int)(seq - player->inputSeq) > 0)
      stage->setInput(*player, vec2(mx, my), up, yaw, pitch, flags & 1, seq);
    break;
  }
  case C_USE: {
    unsigned index = in.u16();
    vec3 at = in.vec3();
    const auto &targets = stage->getInteractables();
    if (in.isOk() && index < targets.size())
      stage->useInteractable(*player, *targets[index], at);
    break;
  }
  case C_ACTION: {
    uint8_t action = in.u8();
    if (!in.isOk() || player->dead)
      break;
    stage->performAs(*player, [&]() {
      switch (action) {
      case A_LEAVE: stage->leaveVehicle(*player); break;
      case A_HEADLIGHTS: stage->toggleHeadlights(*player); break;
      case A_ENGINE: stage->toggleEngine(*player); break;
      case A_HANDBRAKE: stage->toggleHandbrake(*player); break;
      case A_VEHICLE_CAMERA: stage->toggleVehicleCamera(*player); break;
      case A_SHIP_LEGS: stage->toggleShipLegs(*player); break;
      case A_REFUEL: stage->refuel(*player); break;
      case A_POP_TIRE: stage->popRandomTire(*player); break;
      default: break;
      }
    });
    break;
  }
  case C_FIRE: {
    vec3 eye = in.vec3(), direction = in.vec3();
    if (in.isOk() && !player->dead)
      stage->performAs(*player, [&]() { stage->fire(*player, eye, direction); });
    break;
  }
  case C_COMMAND: {
    std::string text = in.string();
    if (in.isOk())
      command(*player, text);
    break;
  }
  case C_RELOCATE:
    relocate(in);
    break;
  case C_PROPERTY:
    changeProperty(in);
    break;
  default:
    break;
  }
}

// A snapshot (S_SNAPSHOT) is
//   u32 time (ms), f32 timeOfDay, f32 dayDuration, f32 timeScale
//   the player it is for: u32 last input seq, u8 flags, vec3 abductPoint, f32 paralysis, f32 possessYaw,
//     u32 controlSerial, i32 netId of the character he controls, f32 camera distance, height, yaw
//   u16 n, n x (i32 player id, u8 flags)       all the players
//   u16 n, n x (i32 netId, u16 size, size bytes)       every dynamic object:
//       u8 flags, vec3 position, quat turn, f32 scale, vec3 velocity, then its own writeNetState
void NetServer::sendSnapshots() {
  snapshotCount++;
  for (const Stage::NetEvent &event : stage->takeNetEvents())
    sendSpawn(nullptr, event);
  objects.clear();
  objectCount = 0;
  for (const auto &o : stage->getDynamicObjects()) {
    if (o->getNetId() < 0)
      continue;
    NetWriter blob;
    blob.u8((o->isVisible() ? OBJ_VISIBLE : 0) | (o->isCollidable() ? OBJ_COLLIDABLE : 0));
    blob.vec3(o->getPosition());
    blob.quat(turnOf(o->getRotationMatrix()));
    blob.f32(o->getScale());
    blob.vec3(o->getVelocity());
    o->writeNetState(blob);
    objects.i32(o->getNetId());
    objects.u16((uint16_t)std::min<size_t>(blob.size(), 65535));
    objects.raw(blob.data());
    objectCount++;
  }
  auto flagsOf = [](const Player &p) {
    return (uint8_t)((p.dead ? PLAYER_DEAD : 0) | (p.abducted ? PLAYER_ABDUCTED : 0) |
                     (p.inVehicle ? PLAYER_IN_VEHICLE : 0) | (p.inSaucer ? PLAYER_IN_SAUCER : 0) |
                     (p.possessed ? PLAYER_POSSESSED : 0) | (p.seated ? PLAYER_SEATED : 0));
  };
  for (auto &c : clients) {
    if (c->playerId < 0)
      continue;
    Player *me = stage->findPlayer(c->playerId);
    if (!me)
      continue;
    NetWriter w;
    w.u32((uint32_t)(clock * 1000.0));
    w.f32(stage->getTimeOfDay());
    w.f32(stage->getDayDuration());
    w.f32(stage->getTimeScale());
    w.u32(me->inputSeq);
    w.u8(flagsOf(*me));
    w.vec3(me->abductPoint);
    w.f32(me->paralysis);
    w.f32(me->possessYaw);
    w.u32(me->controlSerial);
    w.i32(me->character ? me->character->getNetId() : -1);
    w.f32(me->cameraDistance);
    w.f32(me->cameraHeight);
    w.f32(me->cameraYaw);
    w.u16((uint16_t)stage->getPlayers().size());
    for (const auto &p : stage->getPlayers()) {
      w.i32(p->id);
      w.u8(flagsOf(*p));
    }
    w.u16(objectCount);
    w.raw(objects.data());
    send(*c, S_SNAPSHOT, w);
  }
}
