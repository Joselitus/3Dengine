#include "Typewriter.h"

#include <algorithm>

using namespace std;

void Typewriter::say(const string &text) {
  length = text.size();
  elapsed = 0.0;
  running = length > 0;
  started = true;
}

void Typewriter::update(double dt, const glm::vec3 &) {
  if (!running)
    return;
  elapsed += dt;
  if (elapsed * charsPerSecond >= length)
    running = false;
}

float Typewriter::progress() const {
  if (!started)
    return 0.0f;
  if (!running || length == 0)
    return 1.0f; // finished, or stopped: show it all
  return (float)min(1.0, elapsed * charsPerSecond / length);
}
