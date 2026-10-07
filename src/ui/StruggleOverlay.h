#ifndef STRUGGLE_OVERLAY
#define STRUGGLE_OVERLAY

#include <functional>
#include <string>

#include "UIOverlay.h"

// While Bob holds the player: a line telling him which key to hammer to get free, and a bar with
// how near he is (the main loop sets `progress`, 0..1, or < 0 to hide it). Registered with
// UIManager::addOverlay.
class StruggleOverlay : public UIOverlay {
  float progress = -1.0f;
  std::function<std::string()> keyName;

public:
  explicit StruggleOverlay(std::function<std::string()> key) : keyName(key) {}
  void setProgress(float p) { progress = p; }
  void draw(UIRenderer &renderer, float width, float height) const override;
};

#endif
