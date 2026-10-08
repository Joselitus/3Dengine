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
  bool lookEnabled = true; // false: the mouse doesn't turn the camera
  bool resync = false;  // next update only reads the cursor (after a pause)
  // What the controls said at the last update (for the client to send to the server)
  glm::vec2 lastDir = glm::vec2(0.0f);
  float lastUp = 0.0f;
  bool lastRunning = false;

public:
  // `controls` must outlive the controller (it is read every frame, so
  // rebinding a key takes effect at once)
  Controller(GLFWwindow *window, Camera *camera, const Controls &controls);
  // Attach to a character: WASD moves it and the camera follows it, looking
  // straight ahead (yaw and pitch reset)
  // cameraYaw: where the view starts looking (radians; 0 = towards -z)
  void attach(PlayableCharacter *character, float cameraDistance,
              float cameraHeight, float cameraYaw = 0.0f);
  // Lets go of the character (it is about to be destroyed: its map is going)
  void detach() { character = nullptr; }
  void update();
  // While disabled (e.g. an interface is open) it ignores the input, the
  // character gets no input and the cursor is free; when enabled again the
  // camera continues from where it was, ignoring where the cursor went.
  void setEnabled(bool enabled);
  bool isEnabled() const { return enabled; }
  // The movement keys (x right, y backwards), the up/down keys and the run key as they were at
  // the last update, and where the camera looks; all at rest while the controller is disabled
  glm::vec2 getMove() const { return enabled ? lastDir : glm::vec2(0.0f); }
  float getUp() const { return enabled ? lastUp : 0.0f; }
  bool isRunning() const { return enabled && lastRunning; }
  float getYaw() const { return yaw; }
  float getPitch() const { return pitch; }
  // While false the mouse doesn't turn the camera (its movement is used for
  // something else, e.g. turning an object in the debug placement mode); the
  // keys still move the character
  void setLookEnabled(bool enable) { lookEnabled = enable; }
};

#endif
