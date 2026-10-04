#ifndef UI_INFO_ROW
#define UI_INFO_ROW

#include <functional>
#include <string>

#include "UIElement.h"

// A line with a text on the left and a value on the right, e.g. an action
// and its key in the ControlsMenu. The value can be a function, read every
// frame (like UILabel), so it always shows the current state.
class UIInfoRow : public UIElement {
private:
  std::string text;
  std::function<std::string()> value;

public:
  UIInfoRow(const std::string &text, const std::string &value);
  UIInfoRow(const std::string &text, std::function<std::string()> value);

  float preferredHeight() const override;
  void draw(UIRenderer &renderer, const UIState &state) const override;
};

#endif
