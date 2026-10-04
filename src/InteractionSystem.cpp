#include "InteractionSystem.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

using namespace std;

#define USE_KEY GLFW_KEY_E

InteractionSystem::InteractionSystem(GLFWwindow *window, UIManager *ui,
                                     Controller *controller)
    : window(window), ui(ui), controller(controller) {}

Interactable *InteractionSystem::closest(const glm::vec3 &player) const {
  Interactable *best = nullptr;
  float bestDistance = 0.0f;
  for (Interactable *target : targets) {
    float range = target->getInteractionRange();
    float d = glm::distance2(player, target->getInteractionPoint());
    if (d <= range * range && (!best || d < bestDistance)) {
      best = target;
      bestDistance = d;
    }
  }
  return best;
}

void InteractionSystem::open(Interactable *target) {
  inUse = target;
  ui->open(*target);
  controller->setEnabled(false);
}

void InteractionSystem::close() {
  inUse = nullptr;
  ui->closeAll();
  controller->setEnabled(true);
}

void InteractionSystem::update(const glm::vec3 &playerPosition) {
  bool use = glfwGetKey(window, USE_KEY) == GLFW_PRESS;
  bool escape = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
  bool usePressed = use && !useWasDown;
  bool escapePressed = escape && !escapeWasDown;
  useWasDown = use;
  escapeWasDown = escape;

  // Closed with the panel's own button
  if (inUse && !ui->hasPanels())
    close();

  if (inUse) {
    if (usePressed || escapePressed)
      close();
  } else {
    Interactable *target = closest(playerPosition);
    if (target && usePressed)
      open(target);
  }

  if (inUse)
    ui->setHint("");
  else if (Interactable *target = closest(playerPosition))
    ui->setHint("E: usar " + target->getInteractionName());
  else
    ui->setHint("");
}
