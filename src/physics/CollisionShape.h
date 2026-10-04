#ifndef COLLISION_SHAPE
#define COLLISION_SHAPE

#include <memory>
#include <vector>

#include <glm/glm.hpp>

// Where an object is, for its collision shape: the shape is described in the
// object's own frame and scaled, rotated and moved by this.
struct Pose {
  glm::vec3 position = glm::vec3(0.0f);
  glm::mat3 rotation = glm::mat3(1.0f);
  float scale = 1.0f;
};

// Result of a collision test between shapes A and B: the smallest push that
// separates them. `normal` points from A to B, so A has to move along
// -normal and B along +normal by `depth` in total.
struct Contact {
  glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
  float depth = 0.0f;
};

// The volume that stands for an object in collisions. Capsule (a pill) is the
// default of every GameObject; Box is for long or flat things, like the RV.
class CollisionShape {
public:
  enum Type { CAPSULE, BOX };

  virtual ~CollisionShape() {}
  virtual Type type() const = 0;

  // Axis-aligned bounds in the world
  virtual void bounds(const Pose &pose, glm::vec3 &min,
                      glm::vec3 &max) const = 0;
  // Points of the lowest part of the shape, in the world, whichever way up it
  // is: they must stay above the floor (a box gives its corners and a grid on
  // its lowest face)
  virtual void floorSamples(const Pose &pose,
                            std::vector<glm::vec3> &out) const = 0;
  // Whether the ray from `origin` along `direction` (unit length) hits the
  // shape; then `distance` is how far along the ray (0 if it starts inside)
  virtual bool raycast(const Pose &pose, const glm::vec3 &origin,
                       const glm::vec3 &direction, float &distance) const = 0;

  // True if A (at poseA) and B (at poseB) overlap; then `contact` is filled
  static bool collide(const CollisionShape &a, const Pose &poseA,
                      const CollisionShape &b, const Pose &poseB,
                      Contact &contact);
};

// A pill: a vertical segment with a radius around it. It spans `height` from
// `base` up (so base is the point under its feet), in the object's frame.
class Capsule : public CollisionShape {
private:
  float radius, height;
  glm::vec3 base;

public:
  Capsule(float radius, float height,
          const glm::vec3 &base = glm::vec3(0.0f));
  // The capsule that fits in the box min..max (object frame)
  static std::shared_ptr<Capsule> fit(const glm::vec3 &min,
                                      const glm::vec3 &max);

  Type type() const override { return CAPSULE; }
  // The ends of the segment of the core and the radius, in the world
  void segment(const Pose &pose, glm::vec3 &a, glm::vec3 &b,
               float &worldRadius) const;
  void bounds(const Pose &pose, glm::vec3 &min, glm::vec3 &max) const override;
  void floorSamples(const Pose &pose,
                    std::vector<glm::vec3> &out) const override;
  bool raycast(const Pose &pose, const glm::vec3 &origin,
               const glm::vec3 &direction, float &distance) const override;
  float getRadius() const { return radius; }
  float getHeight() const { return height; }
  const glm::vec3 &getBase() const { return base; }
};

// An oriented box: halfExtents around `center` (object frame)
class Box : public CollisionShape {
private:
  glm::vec3 halfExtents, center;

public:
  Box(const glm::vec3 &halfExtents, const glm::vec3 &center = glm::vec3(0.0f))
      : halfExtents(halfExtents), center(center) {}

  Type type() const override { return BOX; }
  // Centre and half sizes in the world (the axes are pose.rotation's columns)
  void world(const Pose &pose, glm::vec3 &worldCentre,
             glm::vec3 &worldHalf) const;
  void bounds(const Pose &pose, glm::vec3 &min, glm::vec3 &max) const override;
  void floorSamples(const Pose &pose,
                    std::vector<glm::vec3> &out) const override;
  bool raycast(const Pose &pose, const glm::vec3 &origin,
               const glm::vec3 &direction, float &distance) const override;
  const glm::vec3 &getHalfExtents() const { return halfExtents; }
  const glm::vec3 &getCenter() const { return center; }
};

#endif
