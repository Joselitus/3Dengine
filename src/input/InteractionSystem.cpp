#include "InteractionSystem.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

using namespace std;

InteractionSystem::InteractionSystem(GLFWwindow *window, UIManager *ui,
                                     const Controls &controls)
    : window(window), ui(ui), controls(controls) {}

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

void InteractionSystem::update(const glm::vec3 &playerPosition) {
  bool use = glfwGetKey(window, controls.key(Action::Use)) == GLFW_PRESS;
  bool usePressed = use && !useWasDown;
  useWasDown = use;

  // It may have been closed by Esc or by its close button
  if (panel && !ui->isOpen(panel)) {
    panel = nullptr;
    inUse->onInterfaceClosed();
    inUse = nullptr;
  }

  Interactable *target = closest(playerPosition);
  if (usePressed) {
    if (panel) {
      ui->close(panel);
      panel = nullptr;
      inUse->onInterfaceClosed();
      inUse = nullptr;
    } else if (target && !ui->hasPanels()) {
      panel = ui->open(*target);
      inUse = target;
      target->onInterfaceOpened(playerPosition);
    }
  }

  if (!panel && target && !ui->hasPanels())
    ui->setHint(controls.keyName(Action::Use) + ": " +
                target->getInteractionVerb() + " " +
                target->getInteractionName());
  else
    ui->setHint("");
}
