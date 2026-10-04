#ifndef UI_BUTTON
#define UI_BUTTON

#include <functional>
#include <string>

#include "UIElement.h"

// Clickable button: runs its action when pressed and released over it. Its
// text can be fixed or read every frame (e.g. "Siguiente" that becomes
// "Cerrar"), like UILabel.
class UIButton : public UIElement {
private:
  std::function<std::string()> text;
  std::function<void()> action;

public:
  UIButton(const std::string &text, std::function<void()> action);
  UIButton(std::function<std::string()> text, std::function<void()> action);

  float preferredHeight() const override;
  void draw(UIRenderer &renderer, const UIState &state) const override;
  bool isInteractive() const override { return true; }
  void onRelease(float x, float y, bool inside) override;
};

#endif
