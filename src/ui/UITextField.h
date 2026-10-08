#ifndef UI_TEXT_FIELD
#define UI_TEXT_FIELD

#include <string>

#include "UIElement.h"

// A one-line box to type text in, with a blinking cursor at the end. It gets
// the characters and the editing keys from its panel (UIPanel::onChar /
// onKey, see CommandConsole): add() what is typed, backspace() deletes. Only
// what the font can draw is kept (printable ASCII).
class UITextField : public UIElement {
private:
  std::string text;
  size_t maxLength;
  bool focused = true; // the one that gets the typing: it shows the cursor

public:
  explicit UITextField(size_t maxLength = 200) : maxLength(maxLength) {}

  const std::string &getText() const { return text; }
  void setText(const std::string &value);
  void clear() { text.clear(); }
  void setFocused(bool value) { focused = value; }
  bool isFocused() const { return focused; }
  // Appends a typed character; false if it can't be shown or there is no room
  bool add(unsigned int codepoint);
  void backspace();

  float preferredHeight() const override;
  void draw(UIRenderer &renderer, const UIState &state) const override;
};

#endif
