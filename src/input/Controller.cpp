#include "Controller.h"

#include <glm/glm.hpp>

// How far the camera can look up or down, radians (~72 degrees)
#define MAX_PITCH 1.25f

Controller::Controller(GLFWwindow *window, Camera *camera,
                       const Controls &controls)
    : window(window), camera(camera), controls(controls) {
  glfwGetCursorPos(window, &lastX, &lastY);
}

void Controller::attach(PlayableCharacter *character, float cameraDistance,
                        float cameraHeight, float cameraYaw) {
  this->character = character;
  // A new character (e.g. after a map change) starts looking straight ahead
  // (yaw 0, towards -z) unless told where
  yaw = cameraYaw;
  pitch = 0.0f;
  camera->setAngles(yaw, pitch);
  resync = true;
  character->attachCamera(camera, cameraDistance, cameraHeight);
}

void Controller::setEnabled(bool enable) {
  if (enable == enabled)
    return;
  enabled = enable;
  if (enable) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    // The cursor moved freely meanwhile: don't turn the camera for that
    resync = true;
  } else {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    // The character keeps the last input it got: let go of every key
    if (character) {
      character->control(glm::vec2(0.0f), 0.0f, yaw);
      character->setRunning(false);
    }
  }
}

void Controller::update() {
  if (!enabled)
    return;
  // The camera's angles are the truth: the character it follows may turn it
  // too (the RV's cockpit view turns with the vehicle)
  yaw = camera->getYaw();
  pitch = camera->getPitch();
  // The camera turns by how much the cursor moved since the last frame,
  // times the camera's sensitivity (so it can change while playing)
  double x, y;
  glfwGetCursorPos(window, &x, &y);
  if (resync) {
    lastX = x;
    lastY = y;
    resync = false;
  }
  float sensitivity = lookEnabled ? camera->getSensitivity() : 0.0f;
  float newYaw = yaw + sensitivity * (float)(x - lastX);
  float newPitch = glm::clamp(pitch + sensitivity * (float)(y - lastY),
                              -MAX_PITCH, MAX_PITCH);
  lastX = x;
  lastY = y;
  if (newYaw != yaw || newPitch != pitch) {
    yaw = newYaw;
    pitch = newPitch;
    camera->setAngles(yaw, pitch);
  }

  if (character) {
    glm::vec2 dir(0.0f);
    float up = 0.0f; // no key for it (everything walks under gravity now)
    auto held = [this](Action action) {
      return glfwGetKey(window, controls.key(action)) == GLFW_PRESS;
    };
    if (held(Action::MoveBack)) dir.y += 1.0f;
    if (held(Action::MoveForward)) dir.y -= 1.0f;
    if (held(Action::MoveLeft)) dir.x -= 1.0f;
    if (held(Action::MoveRight)) dir.x += 1.0f;

    character->control(dir, up, yaw);
    // The run key is the leave-vehicle one (Shift): on foot there is no
    // vehicle to leave, and in a vehicle running means nothing
    character->setRunning(held(Action::LeaveVehicle));
  }
}
