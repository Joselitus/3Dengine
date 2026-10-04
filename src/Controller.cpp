#include "Controller.h"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

// Added to the heading so the model's front faces the direction of travel
#define MODEL_FORWARD_OFFSET 0.0f
// Mouse pitch limit in pixels (SENSIVILITY radians each)
#define MAX_PITCH_PIXELS 250.0

Controller::Controller(GLFWwindow *window, Camera *camera)
    : window(window), camera(camera) {
  glfwGetCursorPos(window, &originX, &originY);
}

void Controller::attach(GameObject *character, float cameraDistance,
                        float cameraHeight) {
  this->character = character;
  camera->attachTo(character, cameraDistance, cameraHeight);
}

void Controller::setEnabled(bool enable) {
  if (enable == enabled)
    return;
  enabled = enable;
  if (enable) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPos(window, savedX, savedY);
    // The Esc that closed an interface must not also close the game
    escapeArmed = false;
  } else {
    glfwGetCursorPos(window, &savedX, &savedY);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  }
}

void Controller::update() {
  if (!enabled)
    return;
  // The camera rotation is derived from the cursor position, relative to
  // where the pointer was when the controller was created.
  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);
  xpos -= originX;
  ypos -= originY;
  if (ypos > MAX_PITCH_PIXELS || ypos < -MAX_PITCH_PIXELS) {
    ypos = ypos > 0 ? MAX_PITCH_PIXELS : -MAX_PITCH_PIXELS;
    glfwSetCursorPos(window, originX + xpos, originY + ypos);
  }
  if (xpos != camera->getPhi() || ypos != camera->getTheta())
    camera->rotate(xpos, ypos);
  yaw = SENSIVILITY * (float)xpos;

  if (character) {
    glm::vec2 dir(0.0f);
    float up = 0.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) dir.y += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) dir.y -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) dir.x -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) dir.x += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) up -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) up += 1.0f;

    if (dir != glm::vec2(0.0f)) {
      // Relative to the camera heading (same convention as Camera::move)
      glm::vec2 world = glm::rotate(glm::normalize(dir), yaw);
      character->translate(SPEED * glm::vec3(world.x, 0.0f, world.y));
      facing = std::atan2(world.x, world.y) + MODEL_FORWARD_OFFSET;
      character->setRotation(
          glm::rotate(glm::mat4(1.0f), facing, glm::vec3(0.0f, 1.0f, 0.0f)));
    }
    character->translate(glm::vec3(0.0f, SPEED * up, 0.0f));
    camera->follow();
  }

  bool escape = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
  if (!escape)
    escapeArmed = true;
  if (escape && escapeArmed) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetWindowShouldClose(window, true);
  }
}
