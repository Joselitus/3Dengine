#include "NetClient.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <typeinfo>

#include "Protocol.h"

using namespace glm;
using namespace Net;

namespace {
// "host", "host:port", "[v6]" and "[v6]:port"
void splitAddress(const std::string &address, std::string &host, int &port) {
  host = address;
  port = DEFAULT_PORT;
  size_t colon = address.rfind(':');
  if (!address.empty() && address[0] == '[') {
    size_t close = address.find(']');
    if (close != std::string::npos) {
      host = address.substr(1, close - 1);
      if (colon != std::string::npos && colon > close)
        port = atoi(address.c_str() + colon + 1);
    }
    return;
  }
  // (more than one colon is a bare IPv6 address)
  if (colon != std::string::npos && address.find(':') == colon) {
    host = address.substr(0, colon);
    port = atoi(address.c_str() + colon + 1);
  }
}

// Hours, as the shortest way round the clock from `a` to `b`
float hoursBetween(float a, float b) {
  float d = std::fmod(b - a, 24.0f);
  if (d > 12.0f)
    d -= 24.0f;
  if (d < -12.0f)
    d += 24.0f;
  return d;
}
} // namespace

NetClient::NetClient(const std::string &address, const std::string &name) : playerName(name) {
  std::string host;
  int port;
  splitAddress(address, host, port);
  if (port <= 0 || port > 65535) {
    fail("Puerto no valido");
    return;
  }
  std::string why;
  connection = NetConnection::connectTo(host, port, why);
  if (!connection)
    fail(why);
}

void NetClient::fail(const std::string &why) {
  state = State::Failed;
  error = why;
  if (connection)
    connection->close();
}

void NetClient::disconnect() {
  if (connection)
    connection->close();
  if (state != State::Failed)
    state = State::Failed, error = "Desconectado";
}

void NetClient::update() {
  if (!connection || state == State::Failed)
    return;
  connection->update();
  if (state == State::Connecting && !connection->isConnecting()) {
    if (!connection->isOpen()) {
      fail(connection->getError().empty() ? "No puedo conectar" : connection->getError());
      return;
    }
    NetWriter hello;
    hello.u32(PROTOCOL_VERSION);
    hello.string(playerName);
    connection->send(C_HELLO, hello);
    state = State::Greeting;
  }
  NetConnection::Message message;
  while (state != State::Failed && connection->next(message))
    handle(message);
  if (state != State::Failed && !connection->isOpen())
    fail(connection->getError().empty() ? "Se ha perdido la conexion" : connection->getError());
}

void NetClient::handle(const NetConnection::Message &message) {
  NetReader in(message.payload);
  switch (message.type) {
  case S_WELCOME:
    readWelcome(in);
    break;
  case S_REJECT:
    fail(in.string());
    break;
  case S_JOINED: {
    Peer peer;
    peer.id = in.i32();
    peer.name = in.string();
    peer.bodyNetId = in.i32();
    if (!in.isOk())
      break;
    if (stage) {
      Player &p = stage->addPlayer(peer.id, peer.name, peer.bodyNetId);
      p.walker->setReplica(true);
    } else {
      peers.push_back(peer);
    }
    break;
  }
  case S_LEFT: {
    int id = in.i32();
    if (!in.isOk())
      break;
    if (stage)
      stage->removePlayer(id);
    else
      peers.erase(std::remove_if(peers.begin(), peers.end(), [id](const Peer &p) { return p.id == id; }),
                  peers.end());
    break;
  }
  case S_SNAPSHOT:
    if (stage)
      readSnapshot(in);
    break;
  case S_SPAWN:
  case S_DESPAWN:
    if (stage)
      handleSpawn(message.type == S_SPAWN, in);
    else
      pendingEvents.push_back({message.type == S_SPAWN, message.payload});
    break;
  case S_NOTICE: {
    std::string text = in.string();
    if (in.isOk())
      notices.push_back(text);
    break;
  }
  default:
    break;
  }
}

void NetClient::readWelcome(NetReader &in) {
  uint32_t version = in.u32();
  myId = in.i32();
  mapIndex = in.u16();
  mapName = in.string();
  dynamicCount = in.u32();
  uint16_t n = in.u16();
  peers.clear();
  for (int i = 0; i < n && in.isOk(); i++) {
    Peer peer;
    peer.id = in.i32();
    peer.name = in.string();
    peer.bodyNetId = in.i32();
    peers.push_back(peer);
  }
  if (!in.isOk() || version != PROTOCOL_VERSION) {
    fail("El servidor habla otra version del protocolo");
    return;
  }
  state = State::Ready;
}

bool NetClient::attach(GameStage &s) {
  if (state != State::Ready)
    return false;
  if (s.getDynamicObjects().size() != dynamicCount) {
    fail("El mapa de este cliente no es el del servidor (" + std::to_string(s.getDynamicObjects().size()) +
         " objetos contra " + std::to_string(dynamicCount) + "): hay que compilar las dos mitades de la misma version");
    return false;
  }
  stage = &s;
  for (const Peer &peer : peers) {
    Player &p = s.addPlayer(peer.id, peer.name, peer.bodyNetId);
    if (peer.id == myId)
      s.setLocalPlayer(&p);
  }
  s.makeReplicas();
  for (auto &event : pendingEvents) {
    NetReader in(event.second);
    handleSpawn(event.first, in);
  }
  pendingEvents.clear();
  state = State::Playing;
  return true;
}

// Something appeared (a mosquito, an egg) or went away: the same on this side
void NetClient::handleSpawn(bool spawn, NetReader &in) {
  int netId = in.i32();
  if (!in.isOk())
    return;
  if (spawn) {
    unsigned char kind = in.u8();
    vec3 where = in.vec3();
    float arg = in.f32();
    if (in.isOk() && !stage->findDynamic(netId))
      stage->spawnReplica(kind, netId, where, arg);
  } else if (DynamicGameObject *object = stage->findDynamic(netId)) {
    stage->removeLater(object);
  }
}

NetClient::ObjectRecord *NetClient::recordOf(Snapshot &s, int netId) {
  auto found = s.index.find(netId);
  return found == s.index.end() ? nullptr : &s.objects[found->second];
}

void NetClient::readSnapshot(NetReader &in) {
  Snapshot s;
  uint32_t ms = in.u32();
  if (!haveServerTime) {
    serverTime = ms / 1000.0;
    haveServerTime = true;
  } else {
    serverTime += (int32_t)(ms - lastMilliseconds) / 1000.0; // (the counter wraps)
  }
  lastMilliseconds = ms;
  s.time = serverTime;
  s.timeOfDay = in.f32();
  s.dayDuration = in.f32();
  s.timeScale = in.f32();
  s.ackSeq = in.u32();
  // About this player
  uint8_t myFlags = in.u8();
  vec3 abductPoint = in.vec3();
  float paralysis = in.f32();
  float possessYaw = in.f32();
  uint32_t serial = in.u32();
  int characterId = in.i32();
  float distance = in.f32(), height = in.f32(), yaw = in.f32();
  // All of them
  uint16_t playerCount = in.u16();
  std::vector<std::pair<int, uint8_t>> flags;
  for (int i = 0; i < playerCount && in.isOk(); i++) {
    int id = in.i32();
    uint8_t f = in.u8();
    flags.push_back({id, f});
  }
  uint16_t objectCount = in.u16();
  for (int i = 0; i < objectCount && in.isOk(); i++) {
    ObjectRecord r;
    r.netId = in.i32();
    uint16_t size = in.u16();
    if (!in.isOk() || in.remaining() < size)
      return;
    // (the blob's bytes are the next `size` of the message)
    std::vector<uint8_t> bytes(size);
    for (int k = 0; k < size; k++)
      bytes[k] = in.u8();
    NetReader b(bytes);
    r.flags = b.u8();
    r.position = b.vec3();
    r.turn = b.quat();
    r.scale = b.f32();
    r.velocity = b.vec3();
    if (!b.isOk())
      continue;
    r.extra.assign(bytes.end() - b.remaining(), bytes.end());
    s.index[r.netId] = s.objects.size();
    s.objects.push_back(std::move(r));
  }
  if (!in.isOk())
    return;

  // The players: who is dead, who drives; and what this one controls
  for (auto &entry : flags)
    if (Player *p = stage->findPlayer(entry.first)) {
      p->dead = entry.second & PLAYER_DEAD;
      p->abducted = entry.second & PLAYER_ABDUCTED;
      p->inVehicle = entry.second & PLAYER_IN_VEHICLE;
      p->inSaucer = entry.second & PLAYER_IN_SAUCER;
      p->possessed = entry.second & PLAYER_POSSESSED;
      p->seated = entry.second & PLAYER_SEATED;
    }
  if (Player *me = stage->getLocalPlayer()) {
    me->abductPoint = abductPoint;
    me->paralysis = paralysis;
    me->possessYaw = possessYaw;
    if (serial != me->controlSerial) {
      if (auto character = std::dynamic_pointer_cast<PlayableCharacter>(stage->findDynamicShared(characterId)))
        me->character = character;
      me->cameraDistance = distance;
      me->cameraHeight = height;
      me->cameraYaw = yaw;
      me->controlSerial = serial;
    }
    (void)myFlags;
  }
  Snapshot &kept = (snapshots.push_back(std::move(s)), snapshots.back());
  while (snapshots.size() > 40)
    snapshots.pop_front();
  reconcile(kept);
}

// The server has told us where it had the player's penguin when it got input number `ackSeq`:
// the difference with where ours was then is what has to be corrected
void NetClient::reconcile(const Snapshot &snapshot) {
  Player *me = stage->getLocalPlayer();
  if (!me)
    return;
  bool predicted = me->character == me->walker && !me->dead && !me->possessed;
  if (!predicted) {
    history.clear();
    correction = vec3(0.0f);
    return;
  }
  auto found = snapshot.index.find(me->walker->getNetId());
  if (found == snapshot.index.end())
    return;
  const ObjectRecord &server = snapshot.objects[found->second];
  while (history.size() > 1 && (int)(history[1].seq - snapshot.ackSeq) <= 0)
    history.pop_front();
  if (history.empty() || (int)(history.front().seq - snapshot.ackSeq) > 0)
    return; // (we have no record of that moment: wait for the next)
  vec3 error = server.position - history.front().position;
  if (length(error) > 3.0f) { // (pushed far, or teleported: no smoothing)
    me->walker->setPosition(server.position.x, server.position.y, server.position.z);
    me->walker->setVelocity(vec3(0.0f));
    history.clear();
    correction = vec3(0.0f);
  } else {
    correction = error;
  }
}

void NetClient::recordPrediction(unsigned seq, const vec3 &position) {
  if (seq == 0)
    return;
  if (!history.empty() && history.back().seq == seq) {
    history.back().position = position;
    return;
  }
  history.push_back({seq, position});
  while (history.size() > 240)
    history.pop_front();
}

void NetClient::apply(double dt) {
  if (!stage || snapshots.empty())
    return;
  Player *me = stage->getLocalPlayer();
  Snapshot &newest = snapshots.back();

  // The clock the replicas are shown by: behind the newest snapshot, following it smoothly
  double target = newest.time - INTERPOLATION_DELAY;
  if (!haveRenderTime) {
    renderTime = target;
    haveRenderTime = true;
  } else {
    renderTime += dt;
    double error = target - renderTime;
    if (std::fabs(error) > 0.5)
      renderTime = target;
    else
      renderTime += error * std::min(1.0, dt * 4.0);
  }

  // The time of day (between snapshots it runs by itself)
  if (std::fabs(hoursBetween(stage->getTimeOfDay(), newest.timeOfDay)) > 0.05f)
    stage->setTimeOfDay(newest.timeOfDay);
  if (stage->getDayDuration() != newest.dayDuration)
    stage->setDayDuration(newest.dayDuration);
  if (stage->getTimeScale() != newest.timeScale)
    stage->setTimeScale(newest.timeScale);

  // Which two snapshots the world is between
  int before = -1;
  for (size_t i = 0; i < snapshots.size(); i++)
    if (snapshots[i].time <= renderTime)
      before = (int)i;
  const Snapshot &a = snapshots[std::max(before, 0)];
  const Snapshot &b = before + 1 < (int)snapshots.size() ? snapshots[before + 1] : a;
  float alpha = 0.0f;
  if (before >= 0 && &a != &b && b.time > a.time)
    alpha = (float)clamp((renderTime - a.time) / (b.time - a.time), 0.0, 1.0);

  // The player's own penguin: his, while he walks (and the server's again when it is not)
  bool predicted = me && me->character == me->walker && !me->dead && !me->possessed;
  if (me) {
    if (predicted && !wasPredicted) {
      // He is on foot again (got out, came back from the dead): where the server has him
      if (const ObjectRecord *r = recordOf(const_cast<Snapshot &>(newest), me->walker->getNetId())) {
        me->walker->setPosition(r->position.x, r->position.y, r->position.z);
        me->walker->setVelocity(vec3(0.0f));
      }
      history.clear();
      correction = vec3(0.0f);
    }
    me->walker->setReplica(!predicted);
    wasPredicted = predicted;
    if (predicted) {
      float k = (float)std::min(1.0, dt * 10.0);
      me->walker->translate(correction * k);
      correction *= 1.0f - k;
    }
  }

  // What each snapshot says besides where things are, once its time has come
  for (Snapshot &s : snapshots)
    if (!s.extrasApplied && s.time <= renderTime) {
      s.extrasApplied = true;
      for (const ObjectRecord &r : s.objects) {
        DynamicGameObject *object = stage->findDynamic(r.netId);
        if (!object || r.extra.empty())
          continue;
        NetReader extra(r.extra);
        object->readNetState(extra);
        // The two halves of a class must agree on what is sent: say it once if they do not
        if ((!extra.isOk() || extra.remaining() != 0) && warnedClasses.insert(typeid(*object).name()).second)
          fprintf(stderr, "Red: el estado de un %s no coincide con el del servidor (%zu bytes sin leer%s): "
                          "hay que compilar el cliente y el servidor de la misma version\n",
                  typeid(*object).name(), extra.remaining(), extra.isOk() ? "" : ", y se pidio mas de lo enviado");
      }
    }

  // Where things are: between the two snapshots
  for (const ObjectRecord &rb : b.objects) {
    DynamicGameObject *object = stage->findDynamic(rb.netId);
    if (!object)
      continue;
    bool mine = me && object == me->walker.get();
    if (mine && predicted)
      continue;
    auto found = a.index.find(rb.netId);
    const ObjectRecord &ra = found == a.index.end() ? rb : a.objects[found->second];
    bool jump = length(ra.position - rb.position) > 25.0f; // (it was put somewhere else)
    float t = jump ? 1.0f : alpha;
    vec3 position = mix(ra.position, rb.position, t);
    quat turn = glm::slerp(ra.turn, rb.turn, t);
    object->setNetPose(position, mat4_cast(turn));
    object->setScale(mix(ra.scale, rb.scale, t));
    object->setVelocity(rb.velocity);
    object->setCollidable(rb.flags & OBJ_COLLIDABLE);
    if (!mine)
      object->setVisible(rb.flags & OBJ_VISIBLE);
  }

  // Old snapshots go (keeping the one before the one we are in)
  while (before > 0 && snapshots.size() > 2) {
    snapshots.pop_front();
    before--;
  }
}

unsigned NetClient::sendInput(double dt, const vec2 &move, float up, float yaw, float pitch, bool running) {
  sinceInput += dt;
  if (state != State::Playing || sinceInput < INPUT_INTERVAL)
    return 0;
  sinceInput = 0.0;
  NetWriter w;
  w.u32(++inputSeq);
  w.f32(move.x);
  w.f32(move.y);
  w.f32(up);
  w.f32(yaw);
  w.f32(pitch);
  w.u8(running ? 1 : 0);
  connection->send(C_INPUT, w);
  return inputSeq;
}

void NetClient::sendUse(unsigned index, const vec3 &where) {
  if (state != State::Playing)
    return;
  NetWriter w;
  w.u16((uint16_t)index);
  w.vec3(where);
  connection->send(C_USE, w);
}

void NetClient::sendAction(uint8_t action) {
  if (state != State::Playing)
    return;
  NetWriter w;
  w.u8(action);
  connection->send(C_ACTION, w);
}

void NetClient::sendFire(const vec3 &eye, const vec3 &direction) {
  if (state != State::Playing)
    return;
  NetWriter w;
  w.vec3(eye);
  w.vec3(direction);
  connection->send(C_FIRE, w);
}

void NetClient::sendCommand(const std::string &text) {
  if (state != State::Playing)
    return;
  NetWriter w;
  w.string(text);
  connection->send(C_COMMAND, w);
}
