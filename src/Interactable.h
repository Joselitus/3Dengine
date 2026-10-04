#ifndef INTERACTABLE
#define INTERACTABLE

#include <string>

#include <glm/glm.hpp>

class UIPanel;

// Something in the world the player can walk up to and use through an
// interface. Implemented by game objects (e.g. Satellite); the
// InteractionSystem finds the closest one in range and the UIManager opens
// a panel that the object fills in with buildInterface().
class Interactable {
public:
  virtual ~Interactable() = default;

  // Shown in the prompt and as the panel title
  virtual std::string getInteractionName() const = 0;
  // Where the player has to be close to
  virtual glm::vec3 getInteractionPoint() const = 0;
  virtual float getInteractionRange() const { return 3.0f; }

  // Adds the object's controls to an empty panel. The controls may keep
  // pointers to the object: it must outlive the panel.
  virtual void buildInterface(UIPanel &panel) = 0;

  // Optional hooks, called by the InteractionSystem: right after the panel
  // opens (with where the player is, e.g. to turn to face them), and once it
  // has closed, however it was closed (Use key, Esc, close button).
  virtual void onInterfaceOpened(const glm::vec3 &playerPosition) {}
  virtual void onInterfaceClosed() {}
};

#endif
