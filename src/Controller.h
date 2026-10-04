#ifndef CONTROLLER
#define CONTROLLER

#include "Camera.h"
#include "GameObject.h"

// Turns keyboard and mouse input into actions on the attached game character
// and the camera that follows it.
// Third-person input. The cursor position (relative to where it was at start)
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

public:
  Controller(GLFWwindow *window, Camera *camera);
  // Attach to a character: WASD moves it and the camera follows it
  void attach(GameObject *character, float cameraDistance, float cameraHeight);
  void update();
};

#endif
