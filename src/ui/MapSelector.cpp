#include "MapSelector.h"

#include "Controls.h"
#include "UIButton.h"
#include "UILabel.h"

using namespace std;

MapSelector::MapSelector(const vector<string> &mapNames, int current,
                         int closeKey, function<void(int)> select)
    : UIPanel("Mapas (debug)", 300.0f, false), closeKey(closeKey) {
  for (size_t i = 0; i < mapNames.size(); i++) {
    int index = (int)i;
    string text = mapNames[i] + (index == current ? "  (actual)" : "");
    add(new UIButton(text, [this, index, select]() {
      select(index);
      requestClose();
    }));
  }
  add(new UILabel(Controls::keyName(closeKey) + " o Esc: cerrar",
                  UITheme::MUTED));
}

bool MapSelector::onKey(int key) {
  if (key != closeKey)
    return false;
  requestClose();
  return true;
}
