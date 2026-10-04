#ifndef CONTROLLER
#define CONTROLLER

#include "Camera.h"
#include "PlayableCharacter.h"

// Third-person input. The cursor position (relative to where it was at start)
// sets the camera yaw/pitch, which is handed to the attached character
// together with the WASD / Space / Left Shift input: what they do is up to the
// PlayableCharacter (see RV). Esc closes the window. Call update() once per
// frame.
class Controller {
private:
  GLFWwindow *window;
  Camera *camera;
  PlayableCharacter *character = nullptr;

  double originX, originY; // cursor position on the first frame
  float yaw = 0.0f;        // camera heading, radians

public:
  Controller(GLFWwindow *window, Camera *camera);
  // Attach to a character: WASD moves it and the camera follows it
  void attach(PlayableCharacter *character, float cameraDistance, float cameraHeight);
  void update();
};

#endif
