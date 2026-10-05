#include "PropertyPanel.h"

#include "GameObject.h"
#include "UIButton.h"
#include "UILabel.h"
#include "UISlider.h"

using namespace std;

PropertyPanel::PropertyPanel(const string &title, shared_ptr<GameObject> object)
    : UIPanel(title, 440.0f), object(object) {
  object->getProperties(properties);
  build();
}

PropertyPanel::PropertyPanel(const string &title, const vector<Property> &properties)
    : UIPanel(title, 440.0f), properties(properties) {
  build();
}

void PropertyPanel::build() {
  for (const Property &p : properties) {
    switch (p.kind) {
    case Property::Kind::Number:
      if (p.set) {
        add(new UISlider(p.name, p.min, p.max, p.step, p.get, p.set, p.unit));
        break;
      }
      // read only: a line of text, like Info
      add(new UILabel([p]() { return p.name + ": " + p.valueText(); }));
      break;
    case Property::Kind::Info:
      add(new UILabel([p]() { return p.name + ": " + p.valueText(); }));
      break;
    case Property::Kind::Toggle:
      if (p.set)
        add(new UIButton([p]() { return p.name + ": " + p.valueText(); },
                         [p]() { p.set(p.get() > 0.5f ? 0.0f : 1.0f); }));
      else
        add(new UILabel([p]() { return p.name + ": " + p.valueText(); }));
      break;
    case Property::Kind::Action:
      add(new UIButton(p.name, p.run));
      break;
    }
  }
  if (properties.empty())
    add(new UILabel("No tiene propiedades que cambiar.", UITheme::MUTED));
  add(new UIButton("Cerrar", [this]() { requestClose(); }));
}
