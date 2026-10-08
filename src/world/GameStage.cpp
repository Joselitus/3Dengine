#include "GameStage.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "FollaCulos.h"
#include "NetRole.h"
#include "Npc.h"

using namespace std;
using namespace glm;

constexpr unsigned int GameStage::AVATAR_ANIMATION;
constexpr float GameStage::AVATAR_GRAVITY;
constexpr double GameStage::RESPAWN_DELAY;


void GameStage::setSky(shared_ptr<Model> model, int unlit) {
  sky = make_shared<GameObject>();
  sky->addPart(model, unlit);
}

float GameStage::groundAt(float x, float z, float fallback) const {
  float height;
  return floorAt(x, z, height) ? height : fallback;
}

void GameStage::render(Shader *shader, const vec3 &cameraPosition,
                       double time) {
  if (sky) {
    shader->setFloat("time", (float)time); // star twinkle
    glDisable(GL_DEPTH_TEST);
    sky->setPosition(cameraPosition.x, cameraPosition.y, cameraPosition.z);
    sky->Draw(shader);
    glEnable(GL_DEPTH_TEST);
  }
  setDrawOrigin(cameraPosition);
  Draw(shader, time);
}

// ----------------------------------------------------------------- players
vec3 GameStage::spawnPosition(int index) const {
  // The first one on the spawn point, the others round it
  float angle = 2.4f * index, radius = index == 0 ? 0.0f : 1.4f + 0.35f * std::min(index, 10);
  vec3 at = spawnPoint + vec3(std::sin(angle), 0.0f, std::cos(angle)) * radius;
  at.y = groundAt(at.x, at.z, spawnPoint.y);
  return at;
}

Player &GameStage::addPlayer(int id, const string &name, int netId) {
  unique_ptr<Player> p(new Player());
  p->id = id;
  p->name = name;
  p->walker = make_shared<Walker>(
      make_shared<AnimatedModel>(avatarModel.c_str(), true, AVATAR_ANIMATION));
  vec3 at = spawnPosition((int)players.size());
  p->walker->setPosition(at.x, at.y, at.z);
  p->walker->setYaw(spawnYaw + 3.14159265f); // (the camera's yaw 0 looks towards -z, the body's towards +z)
  p->walker->setGravity(AVATAR_GRAVITY);
  addDynamic(p->walker, netId);
  setControl(*p, p->walker, walkCameraDistance, walkCameraHeight, spawnYaw);
  players.push_back(std::move(p));
  return *players.back();
}

void GameStage::removePlayer(int id) {
  for (size_t i = 0; i < players.size(); i++)
    if (players[i]->id == id) {
      Player &p = *players[i];
      if (acting == &p)
        acting = nullptr;
      if (local == &p)
        local = nullptr;
      if (!p.dead)
        onPlayerGone(p); // (he lets go of what he drives)
      removeLater(p.walker.get());
      players.erase(players.begin() + i);
      return;
    }
}

Player *GameStage::findPlayer(int id) {
  for (auto &p : players)
    if (p->id == id)
      return p.get();
  return nullptr;
}

void GameStage::killPlayer(Player &p) {
  if (p.dead)
    return;
  p.dead = true;
  p.abducted = false;
  p.timeDead = 0.0;
  p.moveDir = vec2(0.0f);
  if (netRole() == NetRole::Server)
    printf("[juego] %s ha muerto\n", p.name.c_str());
  onPlayerGone(p);
  p.walker->control(vec2(0.0f), 0.0f, 0.0f);
  p.walker->setCollidable(false); // (the body lies there: nothing bumps into it)
}

void GameStage::abductPlayer(Player &p, const vec3 &into) {
  if (p.dead)
    return;
  killPlayer(p);
  if (netRole() == NetRole::Server)
    printf("[juego] %s se lo ha llevado Bob\n", p.name.c_str());
  p.abducted = true;
  p.abductPoint = into;
  p.walker->setVisible(false); // (he is in the beam)
}

void GameStage::respawn(Player &p) {
  if (netRole() == NetRole::Server)
    printf("[juego] %s vuelve a empezar\n", p.name.c_str());
  p.dead = false;
  p.abducted = false;
  p.timeDead = 0.0;
  int index = 0;
  for (size_t i = 0; i < players.size(); i++)
    if (players[i].get() == &p)
      index = (int)i;
  p.walker->setVisible(true);
  p.walker->setCollidable(true);
  p.walker->setGravity(AVATAR_GRAVITY);
  relocate(*p.walker, spawnPosition(index));
  p.walker->setYaw(spawnYaw + 3.14159265f);
  setControl(p, p.walker, walkCameraDistance, walkCameraHeight, spawnYaw);
}

void GameStage::setInput(Player &p, const vec2 &move, float up, float yaw, float pitch,
                         bool running, unsigned seq) {
  // (a client with a broken or hostile program may send anything)
  auto sane = [](float v, float limit) { return std::isfinite(v) ? clamp(v, -limit, limit) : 0.0f; };
  p.moveDir = vec2(sane(move.x, 1.0f), sane(move.y, 1.0f));
  p.moveUp = sane(up, 1.0f);
  p.lookYaw = sane(yaw, 1000.0f);
  p.lookPitch = sane(pitch, 1.6f);
  p.running = running;
  p.inputSeq = seq;
}

void GameStage::tick(double dt) {
  for (auto &p : players) {
    if (p->dead) {
      p->timeDead += dt;
      if (p->timeDead >= RESPAWN_DELAY)
        respawn(*p);
      continue;
    }
    // What his controls say goes to what he controls (nothing while he can't move or look)
    bool still = isImmobilized(*p);
    if (p->character) {
      p->character->control(still ? vec2(0.0f) : p->moveDir, still ? 0.0f : p->moveUp, p->lookYaw);
      p->character->setRunning(!still && p->running);
    }
    p->walker->setLook(p->lookYaw, p->lookPitch);
  }
  onTick(dt);
  update(dt);
}

void GameStage::useInteractable(Player &p, Interactable &target, const vec3 &at) {
  // The server does not take the client's word for it: the thing has to be one that acts at once
  // and free to use; the player has to be where his client says (more or less: the server has
  // him a moment behind) and in reach of it from there
  if (p.dead || !canInteract(p) || !target.usesDirectly() || !target.isInteractionAvailable())
    return;
  if (!std::isfinite(at.x + at.y + at.z) || distance(at, p.getPosition()) > 2.5f)
    return;
  if (distance(at, target.getInteractionPoint()) > target.getInteractionRange() + 0.25f)
    return;
  performAs(p, [&]() { target.onUse(at); });
}

bool GameStage::edgeCullExempt(const GameObject &object) const {
  for (const auto &p : players)
    if (&object == p->walker.get() || &object == p->character.get())
      return true;
  return false;
}

void GameStage::getSpotLights(vector<SpotLight> &lights) const {
  // (the flashlight of the player at this computer first: the shader may not have room for all)
  if (local && local->character == local->walker)
    local->walker->getFlashlight(lights);
  for (const auto &p : players)
    if (p.get() != local && !p->dead && !p->inVehicle && !p->inSaucer)
      p->walker->getFlashlight(lights);
}

void GameStage::makeReplicas() {
  for (auto &o : getDynamicObjects()) {
    o->setReplica(true);
    if (Npc *npc = dynamic_cast<Npc *>(o.get())) {
      npc->setReplicaFloor([this](float x, float z, float &height) { return floorAt(x, z, height); });
      if (FollaCulos *creature = dynamic_cast<FollaCulos *>(npc))
        creature->setNpcLookup([this](int id) { return dynamic_cast<Npc *>(findDynamic(id)); });
    }
  }
}
