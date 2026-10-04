#ifndef RV_CHARACTER
#define RV_CHARACTER

#include <functional>
#include <memory>

#include "Camera.h"
#include "Interactable.h"
#include "ParticleEmitter.h"
#include "PlayableCharacter.h"
#include "VehicleBody.h"

// The RV: W/S drive it and A/D steer its front wheels, so the camera can
// orbit freely without turning the mesh. It is a VehicleBody: a rigid chassis
// on four springs, so it bounces, pitches and rolls over the terrain, and its
// wheels move up and down with the suspension. The camera follows it from
// behind, orbiting around it.
//
// It is also an Interactable: walking up to its door and pressing the Use key
// runs the enter action (set with setEnterAction) straight away, with no
// panel. The stage then gives it the controller and the camera. While it is
// occupied it can't be used again.
class RV : public PlayableCharacter, public Interactable {
protected:
  std::unique_ptr<VehicleBody> body; // created at the first update

private:
  Camera *camera = nullptr;
  float facing = 3.14159265f; // initial heading, radians (pi = towards -z)
  float throttle = 0.0f;      // -1 (reverse) .. 1 (forward)
  float steering = 0.0f;      // -1 (left) .. 1 (right)

  size_t wheelParts[4];              // front -x, front +x, rear -x, rear +x
  bool hasWheels = false;
  bool occupied = false;             // someone is driving it
  std::function<void()> enterAction; // what using the door does
  // Dust thrown up by each wheel (same order as wheelParts) while it drives on
  // sand
  std::vector<std::shared_ptr<ParticleEmitter>> dust;

  void updateDust(const Stage &stage);

  void placeWheels();

public:
  // The RV is a long box, not a pill
  explicit RV(std::shared_ptr<Model> model);

  float getMass() const override;
  // The wheels' dust emitters: give them to the stage (Stage::addEmitter) to
  // have them updated and drawn
  const std::vector<std::shared_ptr<ParticleEmitter>> &getDust() const {
    return dust;
  }
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

  // What happens when the player uses the RV (the stage hands it the controls)
  void setEnterAction(std::function<void()> action) { enterAction = action; }
  void setOccupied(bool occupied) { this->occupied = occupied; }
  // Which way it faces (radians around +y, 0 = towards +z; the door is on its
  // +x side). Only before the first update: after it the physics rules.
  void setHeading(float radians) {
    facing = radians;
    setYaw(radians);
  }
  bool isOccupied() const { return occupied; }
  // World positions: the driver's seat (inside the body) and a point beside
  // the door (on the +x side), `outside` units away from the wall, on the floor
  glm::vec3 seatPosition() const;
  glm::vec3 doorPosition(float outside) const;
  // View directions (camera yaw: 0 looks towards -z, see Controller): along
  // the RV's heading, and away from the door
  float headingYaw() const;
  float doorYaw() const;

  // Interactable: get in at the door
  std::string getInteractionName() const override { return "autocaravana"; }
  std::string getInteractionVerb() const override { return "conducir la"; }
  glm::vec3 getInteractionPoint() const override { return doorPosition(0.4f); }
  float getInteractionRange() const override { return 2.5f; }
  bool isInteractionAvailable() const override { return !occupied; }
  bool usesDirectly() const override { return true; }
  void onUse(const glm::vec3 &playerPosition) override;
  void buildInterface(UIPanel &panel) override {} // no panel

  void attachCamera(Camera *camera, float distance, float height) override;
  void followCamera() override;
  // Stores the input; the camera heading is ignored on purpose
  void control(glm::vec2 dir, float up, float cameraYaw) override;
  void update(double dt) override;
  // The suspension and the floor: moves the RV
  bool contactFloor(const Stage &stage, double dt) override;
};

#endif
