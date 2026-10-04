#ifndef CONTROLLER
#define CONTROLLER

#include "Camera.h"
#include "Controls.h"
#include "PlayableCharacter.h"

// Player input. Mouse movement turns the camera (yaw/pitch, scaled by the
// camera's sensitivity); the yaw is handed to the attached character together
// with the movement keys (Controls: WASD by default): what they do is up to
// the
// PlayableCharacter (see Walker, RV). Esc is not handled here: it opens the
// pause menu (UIManager). Call update() once per frame.
class Controller {
private:
  GLFWwindow *window;
  Camera *camera;
  const Controls &controls; // which key does what
  PlayableCharacter *character = nullptr;

  double lastX, lastY;  // cursor position on the previous frame
  float yaw = 0.0f;     // camera heading, radians
  float pitch = 0.0f;   // camera tilt, radians (positive looks down)

  bool enabled = true;
  bool resync = false;  // next update only reads the cursor (after a pause)

public:
  // `controls` must outlive the controller (it is read every frame, so
  // rebinding a key takes effect at once)
  Controller(GLFWwindow *window, Camera *camera, const Controls &controls);
  // Attach to a character: WASD moves it and the camera follows it, looking
  // straight ahead (yaw and pitch reset)
  void attach(PlayableCharacter *character, float cameraDistance, float cameraHeight);
  void update();
  // While disabled (e.g. an interface is open) it ignores the input, the
  // character gets no input and the cursor is free; when enabled again the
  // camera continues from where it was, ignoring where the cursor went.
  void setEnabled(bool enabled);
  bool isEnabled() const { return enabled; }
};

#endif
