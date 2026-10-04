#ifndef TYPEWRITER
#define TYPEWRITER

#include <string>

#include "LineNarrator.h"

// Silent LineNarrator: reveals the line at a fixed number of characters per
// second, with no sound. Used by Readable objects (signs, notes...).
class Typewriter : public LineNarrator {
private:
  double charsPerSecond;
  double elapsed = 0.0;
  size_t length = 0;
  bool running = false;
  bool started = false; // say() was called at least once

public:
  static constexpr double DEFAULT_SPEED = 40.0; // characters per second

  explicit Typewriter(double charsPerSecond = DEFAULT_SPEED)
      : charsPerSecond(charsPerSecond) {}

  void say(const std::string &text) override;
  void stop() override { running = false; }
  void update(double dt, const glm::vec3 &position) override;

  float progress() const override;
  bool isSpeaking() const override { return running; }
};

#endif
