#ifndef FUEL_PUMP
#define FUEL_PUMP

#include <functional>
#include <memory>

#include "GameObject.h"
#include "Interactable.h"

// A gas station pump: a static object the player on foot can use (E) to open a small panel with
// the tank level of the vehicle and a button that fills it. It knows nothing about the vehicle:
// the stage gives it three functions (the level, the filling, and how far the vehicle is), so
// the pump only works when the vehicle is parked within `reach` metres of it.
class FuelPump : public GameObject, public Interactable {
public:
  struct Tank {
    std::function<float()> level;            // 0..1
    std::function<void()> fill;              // fills it up
    std::function<float(const glm::vec3 &)> distance; // from a point to the vehicle
  };

private:
  Tank tank;
  float reach;

public:
  FuelPump(std::shared_ptr<Model> model, const Tank &tank, float reach = 14.0f);

  std::string getInteractionName() const override { return "Surtidor"; }
  std::string getInteractionVerb() const override { return "repostar en el"; }
  glm::vec3 getInteractionPoint() const override { return position + glm::vec3(0.0f, 1.0f, 0.0f); }
  float getInteractionRange() const override { return 3.5f; }
  void buildInterface(UIPanel &panel) override;
};

#endif
