#ifndef CREATURE_SELECTOR
#define CREATURE_SELECTOR

#include <functional>
#include <vector>

#include "TestAnimation.h"
#include "UIPanel.h"

// The menu of the creature testing tool (Tab): a button for every animation of every creature.
// Choosing one calls `select(creature, animation)` and closes the menu (the caller must not
// change the creature inside that call: it remembers the request and does it later).
class CreatureSelector : public UIPanel {
  int closeKey;

public:
  CreatureSelector(const std::vector<CreatureEntry> &entries, int currentCreature,
                   int currentAnimation, int closeKey, std::function<void(int, int)> select);
  bool dimsBackground() const override { return true; }
  bool onKey(int key) override;
};

#endif
