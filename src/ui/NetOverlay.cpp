#include "NetOverlay.h"

#include <algorithm>

void NetOverlay::showNotice(const std::string &text) {
  notices.push_back({text, NOTICE_TIME});
  if (notices.size() > 5)
    notices.erase(notices.begin());
}

void NetOverlay::update(float dt) {
  for (Notice &n : notices)
    n.left -= dt;
  notices.erase(std::remove_if(notices.begin(), notices.end(), [](const Notice &n) { return n.left <= 0.0f; }),
                notices.end());
}

void NetOverlay::draw(UIRenderer &renderer, float width, float height) const {
  for (const Tag &tag : tags) {
    float tw = UIRenderer::textWidth(tag.text);
    float x = (tag.ndc.x * 0.5f + 0.5f) * width - tw / 2.0f;
    float y = (0.5f - tag.ndc.y * 0.5f) * height - UIRenderer::textHeight();
    renderer.rect(x - 4.0f, y - 2.0f, tw + 8.0f, UIRenderer::textHeight() + 6.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.45f));
    renderer.text(x, y, tag.text, glm::vec4(1.0f, 1.0f, 1.0f, 0.95f));
  }
  float y = 28.0f;
  for (const Notice &n : notices) {
    float tw = UIRenderer::textWidth(n.text);
    float fade = std::min(1.0f, n.left);
    float x = (width - tw) / 2.0f;
    renderer.rect(x - 8.0f, y - 4.0f, tw + 16.0f, UIRenderer::textHeight() + 10.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.6f * fade));
    renderer.text(x, y, n.text, glm::vec4(1.0f, 0.95f, 0.7f, fade));
    y += UIRenderer::textHeight() + 14.0f;
  }
}
