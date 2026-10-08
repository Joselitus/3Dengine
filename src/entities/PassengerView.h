#ifndef PASSENGER_VIEW
#define PASSENGER_VIEW

#include <memory>

#include "PlayableCharacter.h"

class Camera;
class RV;

// What a player controls while he sits in the RV's passenger seat: nothing (the keys do nothing), but
// the camera: it is in his eyes on the copilot's side of the cab and turns, pitches and rolls with the
// vehicle, as the pilot's does (the mouse looks round from there). It is an invisible object of the
// stage that rides on the seat (so that the server and the clients all have it, and the camera
// follows it like any other character); the player's penguin is hidden in the seat meanwhile.
class PassengerView : public PlayableCharacter {
private:
  std::shared_ptr<RV> rv;
  Camera *camera = nullptr;

public:
  explicit PassengerView(std::shared_ptr<RV> rv);

  void attachCamera(Camera *camera, float distance, float height) override;
  void followCamera() override;
  void control(glm::vec2, float, float) override {}
  // It goes where the seat is (the stage does not move it by itself)
  void update(double dt) override;
  bool contactFloor(const Stage &, double) override { return true; }
};

#endif
