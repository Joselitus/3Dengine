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
  // The prompt's verb: "<key>: <verb> <name>", e.g. "E: leer Cartel"
  virtual std::string getInteractionVerb() const { return "usar"; }
  // Where the player has to be close to
  virtual glm::vec3 getInteractionPoint() const = 0;
  virtual float getInteractionRange() const { return 3.0f; }
  // False while it can't be used right now (e.g. a vehicle someone is already
  // driving): it is ignored, with no prompt
  virtual bool isInteractionAvailable() const { return true; }

  // By default using it opens its panel (buildInterface). An object that
  // acts straight away instead (getting into a vehicle) returns true here and
  // gets onUse(); no panel is opened.
  virtual bool usesDirectly() const { return false; }
  virtual void onUse(const glm::vec3 &playerPosition) {}

  // Adds the object's controls to an empty panel. The controls may keep
  // pointers to the object: it must outlive the panel. (Not called if the
  // object usesDirectly(): leave it empty.)
  virtual void buildInterface(UIPanel &panel) = 0;

  // Optional hooks, called by the InteractionSystem: right after the panel
  // opens (with where the player is, e.g. to turn to face them), and once it
  // has closed, however it was closed (Use key, Esc, close button).
  virtual void onInterfaceOpened(const glm::vec3 &playerPosition) {}
  virtual void onInterfaceClosed() {}
};

#endif
