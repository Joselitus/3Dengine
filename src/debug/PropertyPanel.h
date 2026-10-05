#ifndef PROPERTY_PANEL
#define PROPERTY_PANEL

#include <memory>
#include <string>
#include <vector>

#include "Property.h"
#include "UIPanel.h"

class GameObject;

// The debug window to change an object's values (DebugSelector, properties
// mode): one control per Property of the object (GameObject::getProperties),
// a slider for a number, a button that flips a toggle or runs an action, and a
// live line of text for what can only be read. It keeps the object alive while
// it is open, since its controls are bound to it.
class PropertyPanel : public UIPanel {
private:
  std::shared_ptr<GameObject> object;
  std::vector<Property> properties;

public:
  PropertyPanel(const std::string &title, std::shared_ptr<GameObject> object);
};

#endif
