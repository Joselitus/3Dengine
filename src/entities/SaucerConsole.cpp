#include "SaucerConsole.h"

#include <algorithm>

#include "Saucer.h"

using namespace glm;
using namespace std;

SaucerConsole::SaucerConsole(shared_ptr<Saucer> ship, const vector<string> &pages)
    : saucer(ship), dialogue(pages, typewriter) {}

void SaucerConsole::update(double dt) {
  vec3 at = saucer->getPosition();
  if (primed && dt > 0.0) {
    float now = length(at - last) / (float)dt;
    speed += (now - speed) * std::min(1.0f, 6.0f * (float)dt); // (a client's replica moves in small jumps)
  } else {
    speed = 0.0f;
  }
  last = at;
  primed = true;
  typewriter.update(dt, at);
}

// Where the pilot is (the ship itself: the player's position while he flies it)
vec3 SaucerConsole::getInteractionPoint() const { return saucer->getPosition(); }

float SaucerConsole::getInteractionRange() const { return Saucer::RADIUS; }
