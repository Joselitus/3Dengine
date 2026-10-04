#ifndef SATELLITE
#define SATELLITE

#include <memory>

#include "GameObject.h"
#include "Interactable.h"

// A sky-pointing device on an alt-azimuth mount, like a satellite dish or a
// telescope: a cube (the head, whose +y face is the boresight) on a post.
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
// The Satellite object is the head. The post doesn't turn, so it is a second
// GameObject (getMount()) that has to be added to the stage as well.
class Satellite : public GameObject, public Interactable {
private:
  std::shared_ptr<GameObject> mount; // the post, fixed
  glm::vec3 base;                    // centre of the head

  float azimuth = 0.0f, zenith = 0.0f;
  float targetAzimuth = 0.0f, targetZenith = 0.0f;
  float slewRate = 30.0f; // degrees per second, per axis

  void applyOrientation();

public:
  static constexpr float MAX_ZENITH = 90.0f;
  static constexpr float MIN_SLEW_RATE = 1.0f;
  static constexpr float MAX_SLEW_RATE = 120.0f;

  // `ground`: point on the floor where the post stands; `size`: side of the
  // head
  Satellite(std::shared_ptr<Model> cube, glm::vec3 ground, float size = 0.7f);
  std::shared_ptr<GameObject> getMount() const { return mount; }
  // The post goes with it
  void teleport(const glm::vec3 &position) override;
  // Turns its azimuth (and the target's) at once; the post stays
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

  // Interactable
  std::string getInteractionName() const override { return "Satelite"; }
  glm::vec3 getInteractionPoint() const override { return base; }
  void buildInterface(UIPanel &panel) override;
};

#endif
