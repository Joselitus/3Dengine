#ifndef SATELLITE
#define SATELLITE

#include <memory>

#include "GameObject.h"
#include "Interactable.h"

// A sky-pointing device on an alt-azimuth mount: a satellite dish (modelled in
// Blender, assets/antenna/: see split_antenna.py). The head is the dish with
// its arm and receiver (the boresight is along the arm, +y of its model); it
// tilts around a hinge on the dish's lowest edge, on the edge of the base, and
// the base turns with the azimuth like a turntable, carrying the head.
//
// Angles are in degrees:
//   azimuth: heading of the boresight, from north (-z) clockwise to east (+x),
//            in [0, 360)
//   zenith:  angle between the boresight and the vertical, in [0, 90]:
//            0 points straight up, 90 at the horizon (elevation = 90 - zenith)
//
// Commands set a target; Update() turns both axes towards it at no more than
// the slew rate, along the shortest way for the azimuth, like a real mount.
// The player controls it through its interface (Interactable).
//
// The Satellite object is the head. The base moves differently (it doesn't
// tilt), so it is a second GameObject (getMount()) that has to be added to the
// stage as well.
class Satellite : public GameObject, public Interactable {
private:
  std::shared_ptr<GameObject> mount; // the base, turning with the azimuth
  glm::vec3 ground;                  // where the base stands (its bottom centre)
  float size;                        // scale of both models

  float azimuth = 0.0f, zenith = 0.0f;
  float targetAzimuth = 0.0f, targetZenith = 0.0f;
  float slewRate = 30.0f; // degrees per second, per axis

  void applyOrientation();

public:
  static constexpr float MAX_ZENITH = 90.0f;
  static constexpr float MIN_SLEW_RATE = 1.0f;
  static constexpr float MAX_SLEW_RATE = 120.0f;

  // `dish`: antenna_dish.obj, `base`: antenna_base.obj; `ground`: point on
  // the floor where the base stands; `size`: scale of the models (1 = as
  // modelled, a dish 4.5 m wide)
  Satellite(std::shared_ptr<Model> dish, std::shared_ptr<Model> base,
            glm::vec3 ground, float size = 0.5f);
  std::shared_ptr<GameObject> getMount() const { return mount; }
  // The base goes with it
  void teleport(const glm::vec3 &position) override;
  // Turns its azimuth (and the target's) at once, base included
  void turn(float radians) override;
  // From its azimuth (its rotation also has the tilt of the zenith)
  float getHeading() const override;

  // Target orientation; the azimuth is wrapped and the zenith clamped
  void pointAt(float azimuth, float zenith);
  void setTargetAzimuth(float azimuth) { pointAt(azimuth, targetZenith); }
  void setTargetZenith(float zenith) { pointAt(targetAzimuth, zenith); }
  // Stops where it is now
  void stop() { pointAt(azimuth, zenith); }
  void setSlewRate(float degreesPerSecond);

  float getAzimuth() const { return azimuth; }
  float getZenith() const { return zenith; }
  float getElevation() const { return MAX_ZENITH - zenith; }
  float getTargetAzimuth() const { return targetAzimuth; }
  float getTargetZenith() const { return targetZenith; }
  float getSlewRate() const { return slewRate; }
  bool isSlewing() const;
  // Unit vector, in world space, the boresight points along
  glm::vec3 getPointing() const;

  void update(double dt) override;
  // Adds where it points and where it is turning to, and how fast
  void getProperties(std::vector<Property> &properties) override;

  // Interactable
  std::string getInteractionName() const override { return "Satelite"; }
  // Above the middle of the base
  glm::vec3 getInteractionPoint() const override {
    return ground + glm::vec3(0.0f, 1.0f, 0.0f);
  }
  void buildInterface(UIPanel &panel) override;
};

#endif
