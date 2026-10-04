#ifndef CONTROLLER
#define CONTROLLER

#include "Camera.h"
#include "PlayableCharacter.h"

// Reads the keyboard and mouse: it turns the camera and hands the movement
// input to the attached character, which decides what to do with it.
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
