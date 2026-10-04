#ifndef RV_CHARACTER
#define RV_CHARACTER

#include <memory>

#include "Camera.h"
#include "PlayableCharacter.h"
#include "VehicleBody.h"

// The RV: W/S drive it and A/D steer its front wheels, so the camera can
// orbit freely without turning the mesh. It is a VehicleBody: a rigid chassis
// on four springs, so it bounces, pitches and rolls over the terrain, and its
// wheels move up and down with the suspension. The camera follows it from
// behind, orbiting around it.
class RV : public PlayableCharacter {
protected:
  std::unique_ptr<VehicleBody> body; // created at the first update

private:
  Camera *camera = nullptr;
  float facing = 3.14159265f; // initial heading, radians (pi = towards -z)
  float throttle = 0.0f;      // -1 (reverse) .. 1 (forward)
  float steering = 0.0f;      // -1 (left) .. 1 (right)

  size_t wheelParts[4];              // front -x, front +x, rear -x, rear +x
  bool hasWheels = false;

  void placeWheels();

public:
  // The RV is a long box, not a pill
  explicit RV(std::shared_ptr<Model> model);

  float getMass() const override;
  // The body of the vehicle is moved too
  void applyCollision(const glm::vec3 &push,
                      const glm::vec3 &velocityChange) override;

  // The physics of the RV model (see rv.obj), for the given gravity (m/s^2)
  // and top speed (m/s)
  static VehicleBody::Params vehicleParams(float gravity, float maxSpeed);

  // The wheels are separate parts, so they can follow the suspension: a
  // model for each side, centred on the wheel's axle (see generate_rv.py)
  void setWheelModels(std::shared_ptr<Model> negativeX,
                      std::shared_ptr<Model> positiveX);

  void attachCamera(Camera *camera, float distance, float height) override;
  void followCamera() override;
  // Stores the input; the camera heading is ignored on purpose
  void control(glm::vec2 dir, float up, float cameraYaw) override;
  void update(double dt) override;
  // The suspension and the floor: moves the RV
  bool contactFloor(const Stage &stage, double dt) override;
};

#endif
