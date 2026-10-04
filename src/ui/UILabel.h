#ifndef UI_LABEL
#define UI_LABEL

#include <functional>
#include <string>

#include "UIElement.h"

// A line of text. Either fixed, or produced by a function every frame so it
// can show live values (e.g. the current angle of an object).
class UILabel : public UIElement {
private:
  std::function<std::string()> source;
  glm::vec4 color;

public:
  UILabel(const std::string &text, glm::vec4 color = UITheme::TEXT);
  UILabel(std::function<std::string()> source,
          glm::vec4 color = UITheme::TEXT);

  float preferredHeight() const override;
  void draw(UIRenderer &renderer, const UIState &state) const override;
};

#endif
