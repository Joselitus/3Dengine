#include "FuelPump.h"

#include <cstdio>

#include "UIButton.h"
#include "UILabel.h"
#include "UIPanel.h"

using namespace glm;
using std::string;

FuelPump::FuelPump(std::shared_ptr<Model> model, const Tank &tank, float reach)
    : GameObject(model), tank(tank), reach(reach) {}

void FuelPump::buildInterface(UIPanel &panel) {
  panel.add(new UILabel([this]() {
    char text[64];
    snprintf(text, sizeof text, "Deposito: %.0f %%", tank.level() * 100.0f);
    return string(text);
  }));
  panel.add(new UILabel(
      [this]() {
        if (tank.distance(position) > reach)
          return string("Acerca la autocaravana al surtidor");
        return tank.level() >= 0.999f ? string("Deposito lleno") : string("Lista para repostar");
      },
      UITheme::MUTED));
  panel.add(new UIButton("Llenar el deposito", [this]() {
    if (tank.distance(position) <= reach)
      tank.fill();
  }));
}
