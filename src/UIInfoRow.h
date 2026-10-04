#ifndef UI_INFO_ROW
#define UI_INFO_ROW

#include <functional>
#include <string>

#include "UIElement.h"

// A line with a text on the left and a value on the right, e.g. an action
// and its key in the ControlsMenu. The value can be a function, read every
// frame (like UILabel), so it always shows the current state. With an
// `onClick` it is clickable (highlighted under the mouse), e.g. to rebind.
class UIInfoRow : public UIElement {
private:
  std::string text;
  std::function<std::string()> value;
  std::function<void()> onClick;

public:
  UIInfoRow(const std::string &text, const std::string &value);
  UIInfoRow(const std::string &text, std::function<std::string()> value,
            std::function<void()> onClick = nullptr);

  float preferredHeight() const override;
  void draw(UIRenderer &renderer, const UIState &state) const override;
  bool isInteractive() const override { return bool(onClick); }
  void onRelease(float x, float y, bool inside) override;
};

#endif
