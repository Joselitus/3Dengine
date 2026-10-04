#ifndef UI_TEXT_BLOCK
#define UI_TEXT_BLOCK

#include <functional>
#include <string>

#include "UIElement.h"

// A paragraph: text (read every frame) wrapped to the element's width over a
// fixed number of lines (the height must be known before the width, see
// UIPanel). Optionally only its first part is shown: `visible` returns the
// fraction (0..1) to show, e.g. a voice's progress, so subtitles appear as
// they are spoken. Text that doesn't fit is cut, ending in "...".
class UITextBlock : public UIElement {
private:
  std::function<std::string()> source;
  std::function<float()> visible;
  int lines;
  glm::vec4 color;

public:
  UITextBlock(std::function<std::string()> source, int lines,
              std::function<float()> visible = nullptr,
              glm::vec4 color = UITheme::TEXT);

  float preferredHeight() const override;
  void draw(UIRenderer &renderer, const UIState &state) const override;
};

#endif
