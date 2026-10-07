#include "MosquitoEgg.h"

#include "TextFormat.h"

MosquitoEgg::MosquitoEgg(std::shared_ptr<Model> model, std::function<void(MosquitoEgg &)> hatch)
    : DynamicGameObject(model), onHatch(hatch) {
  setGravity(0.0f);
  setCollidable(false);
}

void MosquitoEgg::update(double dt) {
  GameObject::update(dt); // (it does not move)
  age += (float)dt;
  if (!hatched && age >= HATCH_TIME) {
    hatched = true;
    if (onHatch)
      onHatch(*this);
  }
}

void MosquitoEgg::describe(std::vector<std::string> &lines) const {
  DynamicGameObject::describe(lines);
  lines.push_back(hatched ? std::string("Huevo de mosquito: eclosionado")
                          : textFormat("Huevo de mosquito: eclosiona en %.1f s", HATCH_TIME - age));
}
