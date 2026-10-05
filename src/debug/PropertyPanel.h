#ifndef PROPERTY_PANEL
#define PROPERTY_PANEL

#include <memory>
#include <string>
#include <vector>

#include "Property.h"
#include "UIPanel.h"

class GameObject;

// The debug window to change an object's values (DebugSelector, properties
// mode): one control per Property of the object (GameObject::getProperties, or
// Stage::getProperties for the world),
// a slider for a number, a button that flips a toggle or runs an action, and a
// live line of text for what can only be read. It keeps the object alive while
// it is open, since its controls are bound to it (a stage can't be kept: the
// map change closes every panel before replacing it).
class PropertyPanel : public UIPanel {
private:
  std::shared_ptr<GameObject> object; // may be null
  std::vector<Property> properties;

public:
  PropertyPanel(const std::string &title, std::shared_ptr<GameObject> object);
  PropertyPanel(const std::string &title, const std::vector<Property> &properties);

private:
  void build();
};

#endif
