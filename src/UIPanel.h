#ifndef UI_PANEL
#define UI_PANEL

#include <string>

#include "UIContainer.h"

// A window: title bar with a close button, and its children stacked
// vertically below. It can be dragged by the title bar. Its top left corner
// is set with moveTo(); the width is fixed and the height follows the content.
class UIPanel : public UIContainer {
private:
  std::string title;
  float width;
  bool closeRequested = false;
  bool dragging = false;
  float grabX = 0.0f, grabY = 0.0f; // cursor offset from the corner

  UIRect titleBar() const;
  UIRect closeButton() const;

public:
  UIPanel(const std::string &title, float width = 360.0f);

  void moveTo(float x, float y);
  float getWidth() const { return width; }
  // Lays the children out again (call after adding children or moving)
  void relayout() { layout(rect.x, rect.y, width); }
  // Set when the close button is clicked; the UIManager removes the panel
  bool wantsToClose() const { return closeRequested; }
  void requestClose() { closeRequested = true; }
  const std::string &getTitle() const { return title; }

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
