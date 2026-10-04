#ifndef UI_PANEL
#define UI_PANEL

#include <string>

#include "UIContainer.h"

// A window: title bar with a close button, and its children stacked
// vertically below. It can be dragged by the title bar. Its top left corner
// is set with moveTo(); the width is fixed and the height follows the content.
// Subclass it to make a reusable window with its own controls and keys (see
// PauseMenu, OptionsMenu): add the children in the constructor and override
// onKey() for shortcuts.
class UIPanel : public UIContainer {
private:
  std::string title;
  float width;
  bool closable;
  bool closeRequested = false;
  bool dragging = false;
  float grabX = 0.0f, grabY = 0.0f; // cursor offset from the corner

  UIRect titleBar() const;
  UIRect closeButton() const;

public:
  // `closable`: shows the close button in the title bar
  UIPanel(const std::string &title, float width = 360.0f,
          bool closable = true);

  void moveTo(float x, float y);
  float getWidth() const { return width; }
  // Lays the children out again (call after adding children or moving)
  void relayout() { layout(rect.x, rect.y, width); }
  // Set when the close button is clicked; the UIManager removes the panel
  bool wantsToClose() const { return closeRequested; }
  void requestClose() { closeRequested = true; }
  const std::string &getTitle() const { return title; }
  // Darken the game behind while this panel is open (menus)
  virtual bool dimsBackground() const { return false; }

  float preferredHeight() const override;
  void layout(float x, float y, float width) override;
  void draw(UIRenderer &renderer, const UIState &state) const override;

  // The panel itself handles the title bar (drag) and the close button
  bool isInteractive() const override { return true; }
  void onPress(float x, float y) override;
  void onDrag(float x, float y) override;
  void onRelease(float x, float y, bool inside) override;
};

#endif
