#include "UISlider.h"

#include <cmath>
#include <cstdio>

using namespace std;

#define TRACK_HEIGHT 14.0f
#define HANDLE_WIDTH 8.0f

UISlider::UISlider(const string &label, float min, float max, float step,
                   function<float()> get, function<void(float)> set,
                   const string &unit)
    : label(label), min(min), max(max), step(step), get(get), set(set),
      unit(unit) {}

float UISlider::preferredHeight() const {
  return UIRenderer::textHeight() + 4.0f + TRACK_HEIGHT;
}

UIRect UISlider::track() const {
  UIRect t;
  t.x = rect.x;
  t.y = rect.y + UIRenderer::textHeight() + 4.0f;
  t.w = rect.w;
  t.h = TRACK_HEIGHT;
  return t;
}

void UISlider::draw(UIRenderer &renderer, const UIState &state) const {
  float value = get();
  char number[32];
  snprintf(number, sizeof(number), "%.1f",
           value > -0.05f && value < 0.05f ? 0.0f : value); // not "-0.0"
  string valueText = string(number) + (unit.empty() ? "" : " " + unit);
  renderer.text(rect.x, rect.y, label, UITheme::MUTED);
  renderer.text(rect.x + rect.w - UIRenderer::textWidth(valueText), rect.y,
                valueText, UITheme::TEXT);

  UIRect t = track();
  float fraction = max > min ? (value - min) / (max - min) : 0.0f;
  fraction = fmin(fmax(fraction, 0.0f), 1.0f);
  bool highlighted = state.hovered == this || state.active == this;
  renderer.rect(t.x, t.y, t.w, t.h,
                highlighted ? UITheme::HOVER : UITheme::CONTROL);
  renderer.rect(t.x, t.y, t.w * fraction, t.h,
                glm::vec4(glm::vec3(UITheme::ACCENT) * 0.6f, 1.0f));
  renderer.rect(t.x + (t.w - HANDLE_WIDTH) * fraction, t.y - 2.0f,
                HANDLE_WIDTH, t.h + 4.0f, UITheme::ACCENT);
}

void UISlider::setFromCursor(float x) {
  UIRect t = track();
  float fraction = fmin(fmax((x - t.x) / t.w, 0.0f), 1.0f);
  float value = min + fraction * (max - min);
  if (step > 0.0f)
    value = min + roundf((value - min) / step) * step;
  set(value);
}

void UISlider::onPress(float x, float) { setFromCursor(x); }

void UISlider::onDrag(float x, float) { setFromCursor(x); }
