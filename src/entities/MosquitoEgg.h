#ifndef MOSQUITO_EGG
#define MOSQUITO_EGG

#include <functional>

#include "DynamicGameObject.h"

// An egg of the giant mosquito (assets/mosquito/mosquito_egg.obj), floating on the water where it
// was laid. After HATCH_TIME seconds it hatches: it calls its hatch action once (the stage puts a
// young Mosquito there and takes the egg away). It does not collide and does not fall.
class MosquitoEgg : public DynamicGameObject {
private:
  float age = 0.0f;
  bool hatched = false;
  std::function<void(MosquitoEgg &)> onHatch;

public:
  static constexpr float HATCH_TIME = 5.0f;

  MosquitoEgg(std::shared_ptr<Model> model, std::function<void(MosquitoEgg &)> hatch);

  bool hasHatched() const { return hatched; }
  float getAge() const { return age; }
  void update(double dt) override;
  void describe(std::vector<std::string> &lines) const override;
};

#endif
