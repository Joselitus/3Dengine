#ifndef CONTROLLER
#define CONTROLLER

#include "Camera.h"
#include "PlayableCharacter.h"

// Player input. The cursor position (relative to where it was at start)
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

  bool enabled = true;
  double savedX = 0.0, savedY = 0.0; // cursor when it was disabled
  bool escapeArmed = true; // Esc only quits once seen released

public:
  Controller(GLFWwindow *window, Camera *camera);
  // Attach to a character: WASD moves it and the camera follows it
  void attach(PlayableCharacter *character, float cameraDistance, float cameraHeight);
  void update();
  // While disabled (e.g. an interface is open) it ignores the input, the
  // character gets no input and the cursor is free; enabling it again puts
  // the cursor back where it was, so the camera doesn't jump.
  void setEnabled(bool enabled);
  bool isEnabled() const { return enabled; }
};

#endif
