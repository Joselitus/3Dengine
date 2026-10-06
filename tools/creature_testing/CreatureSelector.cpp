#include "CreatureSelector.h"

#include "UIButton.h"
#include "UILabel.h"

using namespace std;

CreatureSelector::CreatureSelector(const vector<CreatureEntry> &entries, int currentCreature,
                                   int currentAnimation, int closeKey,
                                   function<void(int, int)> select)
    : UIPanel("Criatura y animacion", 380.0f, false), closeKey(closeKey) {
  for (size_t c = 0; c < entries.size(); c++) {
    add(new UILabel(entries[c].name));
    for (size_t a = 0; a < entries[c].animations.size(); a++) {
      int creature = (int)c, animation = (int)a;
      bool current = creature == currentCreature && animation == currentAnimation;
      add(new UIButton(entries[c].animations[a] + (current ? "  (actual)" : ""),
                       [this, creature, animation, select]() {
                         select(creature, animation);
                         requestClose();
                       }));
    }
  }
}

bool CreatureSelector::onKey(int key) {
  if (key != closeKey)
    return false;
  requestClose();
  return true;
}
