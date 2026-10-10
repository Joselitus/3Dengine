#ifndef MAP_SELECTOR
#define MAP_SELECTOR

#include <functional>
#include <string>
#include <vector>

#include "UIPanel.h"

// Debug menu, opened with Z: one button per map. Choosing one calls
// `select(index)` and closes the menu. The caller must not swap the map
// inside that call (the menu's own button is still running): the client only
// asks the server for it (/map N) and the map changes when it reconnects.
// Its own key (Action::Maps, Z by default) or Esc close it.
class MapSelector : public UIPanel {
private:
  int closeKey;

public:
  // `closeKey`: the key that opened it, which also closes it
  MapSelector(const std::vector<std::string> &mapNames, int current,
              int closeKey, std::function<void(int)> select);

  bool dimsBackground() const override { return true; }
  bool onKey(int key) override;
};

#endif
