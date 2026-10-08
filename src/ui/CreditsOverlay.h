#ifndef CREDITS_OVERLAY
#define CREDITS_OVERLAY

#include <string>
#include <vector>

#include "UIOverlay.h"

// The end credits: text that scrolls up the screen from below. The people come from a text file
// (see assets/credits/credits.txt for the format), so adding one never touches the code. The main
// loop calls start() when they should begin and update(dt) every frame; the overlay draws nothing
// before start() nor after the last line has gone off the top. Registered with UIManager::addOverlay.
class CreditsOverlay : public UIOverlay {
public:
  enum class Kind { Title, Role, Name, Gap };
  struct Line {
    Kind kind;
    std::string text;
  };

private:
  std::vector<Line> lines;
  bool running = false;
  float time = 0.0f;   // seconds since start()
  mutable float height = 0.0f; // the window's height when last drawn (for isFinished)

  static const float SCROLL_SPEED; // pixels per second

public:
  // Reads the file; false (and no credits) if it can't be opened
  bool load(const std::string &path);
  void start() { running = true; time = 0.0f; }
  void stop() { running = false; }
  void update(float dt) { if (running) time += dt; }
  bool isRunning() const { return running; }
  // True once everything has scrolled off the top
  bool isFinished() const;
  const std::vector<Line> &getLines() const { return lines; }
  void draw(UIRenderer &renderer, float width, float height) const override;
};

#endif
