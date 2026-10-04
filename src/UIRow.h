#ifndef UI_ROW
#define UI_ROW

#include "UIContainer.h"

// Children side by side, sharing the width equally (e.g. a row of buttons).
class UIRow : public UIContainer {
public:
  float preferredHeight() const override;
  void layout(float x, float y, float width) override;
};

#endif
