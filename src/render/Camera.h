#ifndef CAMERA
#define CAMERA
#define GLM_ENABLE_EXPERIMENTAL

#include "myopengl.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>

#include "GameObject.h"
#include "Shader.h"

// Default mouse sensitivity, radians per pixel (see Camera::setSensitivity)
#define SENSIVILITY 0.005f
#define DEFAULT_FOV 45.0f // vertical, degrees
#define SPEED 0.07f

// Perspective camera (vertical fov 45 degrees by default, adjustable; near
// 0.1, far 300) that writes its
// matrices to the shader. Note the shader computes
// projection * model * view * world: here `view` is the translation to the
// camera position and `model` is the camera rotation (pitch * yaw), applied
// after it. With attachTo() the camera follows a GameObject (from behind, or
// from its eyes in first person when the distance is 0) at a
// given distance and height (follow() must be called after it moves).
class Camera {
private:
  GLFWwindow *window;
  Shader *shader;

  int screenWidth;
  int screenHeight;

  glm::vec3 position;
  glm::vec2 rotation; // yaw, pitch in radians (relative to `base`)
  glm::mat4 base = glm::mat4(1.0f); // view rotation of what carries the camera

  float fov = DEFAULT_FOV;          // vertical field of view, degrees
  float sensitivity = SENSIVILITY;  // radians per pixel of mouse movement

  glm::mat4 projection;
  glm::mat4 view;
  glm::mat4 model;

  // Followed object (nullptr = free camera), see attachTo()
  GameObject *target = nullptr;
  float distance = 4.0f; // behind the target; 0 = first person
  float height = 1.5f;   // above the target's origin

public:
  Camera(GLFWwindow *window, Shader *shader);
  void resize();
  void move(float x, float y, float z);
  // Orientation: yaw around +y (positive turns right) and pitch around +x
  // (positive looks down), both in radians
  void setAngles(float yaw, float pitch);
  // The camera rides on something that turns (a vehicle's cockpit): `carrier`
  // is that thing's rotation in the world. The yaw and pitch then are relative
  // to it, so the view follows every tilt and turn of the carrier, whichever
  // way the camera looks. attachTo() puts it back to nothing (identity).
  void setCarrier(const glm::mat3 &carrier);
  float getYaw() const { return rotation.x; }
  float getPitch() const { return rotation.y; }

  // Field of view (vertical, degrees); the projection is rebuilt at once
  void setFov(float degrees);
  float getFov() const { return fov; }
  // How much the camera turns per pixel the mouse moves. Only stored here:
  // the Controller reads it when it turns the camera.
  void setSensitivity(float radiansPerPixel) { sensitivity = radiansPerPixel; }
  float getSensitivity() const { return sensitivity; }
  void reposition(float x, float y, float z);
  void update();
  // Attach the camera to a game object; follow() then keeps it orbiting the
  // object at `distance`, looking at a point `height` above its origin.
  // Follow `target` from `distance` behind it (0: from its eyes, first
  // person) and `height` above its origin
  void attachTo(GameObject *target, float distance, float height);
  void follow();
  glm::vec3 getPosition() { return position; }
  // Size of the camera as a body (a sphere): the main loop keeps it this far
  // above the stage's floor (Stage::keepAboveFloor) so it can't clip under it.
  // Bigger than the near plane (0.1), or the floor would still cut the view.
  static constexpr float RADIUS = 0.3f;
  // For drawing things of its own (particles) the way the world shader does:
  // gl_Position = projection * model * view * world
  glm::mat4 getViewProjection() const { return projection * model * view; }
  // The camera's right and up directions in the world (for billboards)
  glm::vec3 getRight() const { return glm::vec3(model[0][0], model[1][0], model[2][0]); }
  glm::vec3 getUp() const { return glm::vec3(model[0][1], model[1][1], model[2][1]); }
  // World-space direction the camera looks in (e.g. the listener's facing)
  glm::vec3 getForward() const {
    return glm::vec3(glm::inverse(model) * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
  }
};

#endif
