#include "StruggleOverlay.h"

#include <algorithm>

#include "UIRenderer.h"

void StruggleOverlay::draw(UIRenderer &renderer, float width, float height) const {
  if (progress < 0.0f && !possessed)
    return;
  float shown = possessed ? std::max(progress, 0.0f) : progress;
  const float barWidth = 320.0f, barHeight = 18.0f;
  float x = (width - barWidth) / 2.0f, y = height * 0.62f;
  std::string text = possessed ? "Algo controla tu cuerpo. ¡Machaca " + keyName() + " para liberarte!"
                               : "¡Machaca " + keyName() + " para soltarte!";
  float tw = UIRenderer::textWidth(text);
  renderer.rect((width - tw) / 2.0f - 8.0f, y - 30.0f, tw + 16.0f, UIRenderer::textHeight() + 8.0f,
                glm::vec4(0.0f, 0.0f, 0.0f, 0.6f));
  renderer.text((width - tw) / 2.0f, y - 26.0f, text, glm::vec4(1.0f, 0.95f, 0.6f, 1.0f));
  renderer.rect(x - 3.0f, y - 3.0f, barWidth + 6.0f, barHeight + 6.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.7f));
  renderer.rect(x, y, barWidth * glm::clamp(shown, 0.0f, 1.0f), barHeight,
                glm::vec4(0.95f, 0.8f, 0.15f, 0.95f));
}
