#ifndef SAUCER_CONSOLE
#define SAUCER_CONSOLE

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Dialogue.h"
#include "Interactable.h"
#include "Typewriter.h"

class Saucer;

// The on-board console of Bob's ship: something to read (like a Readable, the same dialogue box with
// the text typing itself out, no voice) for the player who flies the ship, while it is still (on the
// ground or hovering: STILL_SPEED). It has no model of its own (it is inside the hull) and is only
// local to each client: the map asks whether its own player is aboard (setAboardQuery) and calls
// update every frame (it measures how fast the ship goes from where it is, which also works for the
// replica a client has).
class SaucerConsole : public Interactable {
public:
  static constexpr float STILL_SPEED = 0.4f; // m/s: slower than this the ship is still

private:
  std::shared_ptr<Saucer> saucer;
  std::function<bool()> aboard;
  Typewriter typewriter;
  Dialogue dialogue; // (after `typewriter`, which delivers its lines)
  glm::vec3 last = glm::vec3(0.0f);
  bool primed = false;
  float speed = 0.0f; // (smoothed)

public:
  SaucerConsole(std::shared_ptr<Saucer> ship, const std::vector<std::string> &pages);
  // Whether the player at this computer flies the ship
  void setAboardQuery(std::function<bool()> q) { aboard = q; }
  void update(double dt);
  bool isStill() const { return primed && speed < STILL_SPEED; }

  // Interactable
  std::string getInteractionName() const override { return "consola de a bordo"; }
  std::string getInteractionVerb() const override { return "leer la"; }
  glm::vec3 getInteractionPoint() const override;
  float getInteractionRange() const override;
  bool isInteractionAvailable() const override { return aboard && aboard() && isStill(); }
  void buildInterface(UIPanel &panel) override { dialogue.buildPanel(panel); }
  void onInterfaceOpened(const glm::vec3 &) override { dialogue.start(); }
  void onInterfaceClosed() override { dialogue.end(); }
};

#endif
