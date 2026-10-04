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

#define SENSIVILITY 0.005f
#define SPEED 0.07f

// Perspective camera (45 degree fov, near 0.1, far 100) that writes its
// matrices to the shader. Note the shader computes
// projection * model * view * world: here `view` is the translation to the
// camera position and `model` is the camera rotation (pitch * yaw), applied
// after it. With attachTo() the camera follows a GameObject from behind at a
// given distance and height (follow() must be called after it moves).
class Camera {
private:
  GLFWwindow *window;
  Shader *shader;

  int screenWidth;
  int screenHeight;

  glm::vec3 position;
  glm::vec2 rotation;

  glm::mat4 projection;
  glm::mat4 view;
  glm::mat4 model;

  // Third-person follow target (nullptr = free camera)
  GameObject *target = nullptr;
  float distance = 4.0f;
  float height = 1.5f;

public:
  Camera(GLFWwindow *window, Shader *shader);
  void resize();
  void move(float x, float y, float z);
  void rotate(int phi, int theta);
  void reposition(float x, float y, float z);
  void update();
  // Attach the camera to a game object; follow() then keeps it orbiting the
  // object at `distance`, looking at a point `height` above its origin.
  void attachTo(GameObject *target, float distance, float height);
  void follow();
  glm::vec3 getPosition() { return position; }
  int getPhi();
  int getTheta();
};

#endif
