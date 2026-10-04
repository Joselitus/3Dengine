#include "Readable.h"

using namespace std;
using namespace glm;

Readable::Readable(shared_ptr<Model> model, const string &name,
                   const vector<string> &pages, float readHeight,
                   double charsPerSecond)
    : GameObject(model), name(name), readHeight(readHeight),
      typewriter(charsPerSecond), dialogue(pages, typewriter) {}

void Readable::update(double dt) {
  GameObject::update(dt);
  typewriter.update(dt, position);
}

vec3 Readable::getInteractionPoint() const {
  return position + vec3(0.0f, readHeight, 0.0f);
}

void Readable::buildInterface(UIPanel &panel) { dialogue.buildPanel(panel); }

void Readable::onInterfaceOpened(const vec3 &) { dialogue.start(); }

void Readable::onInterfaceClosed() { dialogue.end(); }
