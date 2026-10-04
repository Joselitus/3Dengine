#ifndef READABLE
#define READABLE

#include <memory>
#include <string>
#include <vector>

#include "Dialogue.h"
#include "GameObject.h"
#include "Interactable.h"
#include "Typewriter.h"

// Something the player can read: a sign, a note, an inscription... Using it
// (the Use key) opens the same dialogue box as an NPC (see Dialogue), but
// the text is not spoken: a Typewriter reveals it letter by letter, with no
// sound. The panel title is the object's name.
class Readable : public GameObject, public Interactable {
private:
  std::string name;
  float readHeight;  // where the player has to be close to, above position
  Typewriter typewriter;
  Dialogue dialogue; // after `typewriter`, which delivers its lines

public:
  // `pages`: UTF-8 text of each page; `readHeight`: height of the text
  // above the object's position (e.g. a sign's board)
  Readable(std::shared_ptr<Model> model, const std::string &name,
           const std::vector<std::string> &pages, float readHeight = 1.0f,
           double charsPerSecond = Typewriter::DEFAULT_SPEED);

  void update(double dt) override;

  // Interactable
  std::string getInteractionName() const override { return name; }
  std::string getInteractionVerb() const override { return "leer"; }
  glm::vec3 getInteractionPoint() const override;
  void buildInterface(UIPanel &panel) override;
  void onInterfaceOpened(const glm::vec3 &playerPosition) override;
  void onInterfaceClosed() override;
};

#endif
