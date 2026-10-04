#ifndef CONTROLLER
#define CONTROLLER

#include "Camera.h"
#include "GameObject.h"

// Turns keyboard and mouse input into actions on the attached game character
// and the camera that follows it.
// First/third-person input (see Camera::attachTo). The cursor position (relative to where it was at start)
// sets the camera yaw/pitch; WASD moves the attached character relative to
// the camera heading and turns it to face where it walks; Space/Left Shift
// move it up/down; Esc closes the window. Call update() once per frame.
class Controller {
private:
  GLFWwindow *window;
  Camera *camera;
  GameObject *character = nullptr;

  double originX, originY; // cursor position on the first frame
  float yaw = 0.0f;        // camera heading, radians
  float facing = 0.0f;     // character heading, radians

  bool enabled = true;
  double savedX = 0.0, savedY = 0.0; // cursor when it was disabled
  bool escapeArmed = true; // Esc only quits once seen released

public:
  Controller(GLFWwindow *window, Camera *camera);
  // Attach to a character: WASD moves it and the camera follows it
  void attach(GameObject *character, float cameraDistance, float cameraHeight);
  void update();
  // While disabled (e.g. an interface is open) it ignores the input and the
  // cursor is free; enabling it again puts the cursor back where it was, so
  // the camera doesn't jump.
  void setEnabled(bool enabled);
  bool isEnabled() const { return enabled; }
};

#endif
