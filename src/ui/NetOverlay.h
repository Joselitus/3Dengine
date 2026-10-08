#ifndef NET_OVERLAY
#define NET_OVERLAY

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "UIOverlay.h"

// What the multiplayer game shows over the picture: the names of the other players above their
// heads (the main loop gives their places on the screen every frame: setTags), and the messages
// the server sends the player (showNotice), which stay a few seconds at the top. Registered with
// UIManager::addOverlay.
class NetOverlay : public UIOverlay {
public:
  struct Tag {
    std::string text;
    glm::vec2 ndc; // where on the screen: -1..1, y up
  };

private:
  struct Notice {
    std::string text;
    float left;
  };
  std::vector<Tag> tags;
  std::vector<Notice> notices;
  static constexpr float NOTICE_TIME = 5.0f;

public:
  void setTags(const std::vector<Tag> &list) { tags = list; }
  void showNotice(const std::string &text);
  // Lets the messages age
  void update(float dt);
  void draw(UIRenderer &renderer, float width, float height) const override;
};

#endif
