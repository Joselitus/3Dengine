#ifndef UI_SLIDER
#define UI_SLIDER

#include <functional>
#include <string>

#include "UIElement.h"

// Labelled slider for a number in [min, max]. It doesn't store the value:
// it reads it with `get` every frame and writes it with `set` while dragged,
// so it always shows the real state of whatever it controls.
class UISlider : public UIElement {
private:
  std::string label;
  float min, max, step; // step 0 = continuous
  std::function<float()> get;
  std::function<void(float)> set;
  std::string unit;

  UIRect track() const; // the bar, below the label
  void setFromCursor(float x);

public:
  UISlider(const std::string &label, float min, float max, float step,
           std::function<float()> get, std::function<void(float)> set,
           const std::string &unit = "");

  float preferredHeight() const override;
  void draw(UIRenderer &renderer, const UIState &state) const override;
  bool isInteractive() const override { return true; }
  void onPress(float x, float y) override;
  void onDrag(float x, float y) override;
};

#endif
