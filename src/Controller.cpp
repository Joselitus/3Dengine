#include "Controller.h"

#include <glm/glm.hpp>

// Mouse pitch limit in pixels (SENSIVILITY radians each)
#define MAX_PITCH_PIXELS 250.0

Controller::Controller(GLFWwindow *window, Camera *camera)
    : window(window), camera(camera) {
  glfwGetCursorPos(window, &originX, &originY);
}

void Controller::attach(PlayableCharacter *character, float cameraDistance,
                        float cameraHeight) {
  this->character = character;
  character->attachCamera(camera, cameraDistance, cameraHeight);
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
    // The character keeps the last input it got: let go of every key
    if (character)
      character->control(glm::vec2(0.0f), 0.0f, yaw);
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

    character->control(dir, up, yaw);
  }

  bool escape = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
  if (!escape)
    escapeArmed = true;
  if (escape && escapeArmed) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetWindowShouldClose(window, true);
  }
}
