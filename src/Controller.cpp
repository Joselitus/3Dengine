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

void Controller::update() {
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

  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetWindowShouldClose(window, true);
  }
}
