#ifndef LAMP_POST
#define LAMP_POST

#include <memory>

#include "GameObject.h"

// A lamp post of the desert road (assets/lamppost, generate_lamppost.py): a concrete footing, a steel
// pole 7.6 m high and an arm over the road (towards its +z) with the lamp's head, whose glass glows at
// night (setLit). Scenery, solid (a thin capsule). The living one (LampStalker) looks exactly the same
// and hides among them: lineSpot is where it stands to look like one more of the row.
class LampPost : public GameObject {
  size_t glassPart = 0;

public:
  static constexpr float RADIUS = 0.3f, HEIGHT = 8.0f;
  // Along the row (its x: the road's way), how far from it the living one stands to pass for one
  static constexpr float MIMIC_GAP = 8.0f;

  // `post`: lamppost.obj; `glass`: lamppost_glass.obj
  LampPost(std::shared_ptr<Model> post, std::shared_ptr<Model> glass);
  void setLit(bool lit) { setPartUnlit(glassPart, lit ? 2 : 0); }
  // A place in line with it, on one side (`side` +1 or -1), on its ground level
  glm::vec3 lineSpot(int side) const;
};

#endif
