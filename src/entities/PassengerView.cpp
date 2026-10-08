#include "PassengerView.h"

#include "Camera.h"
#include "RV.h"

PassengerView::PassengerView(std::shared_ptr<RV> rv) : rv(rv) {
  setGravity(0.0f);
  setCollidable(false);
  setVisible(false);
}

void PassengerView::attachCamera(Camera *cam, float, float) {
  camera = cam;
  if (!camera)
    return;
  camera->attachTo(nullptr, 0.0f, 0.0f); // (nothing to orbit: followCamera places it)
  camera->setAngles(0.0f, 0.0f);         // (relative to the vehicle: straight ahead)
  followCamera();
}

void PassengerView::followCamera() {
  if (camera)
    rv->placeCockpitCamera(*camera, true);
}

void PassengerView::update(double dt) {
  GameObject::update(dt);
  if (!replica) { // (a copy is put there by the server's snapshots)
    position = rv->copilotSeatPosition();
    rotation = rv->getRotationMatrix();
  }
}
