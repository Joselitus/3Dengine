#ifndef SCENE
#define SCENE

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "AnimatedModel.h"
#include "GameObject.h"
#include "Model.h"
#include "SceneFile.h"
#include "Shader.h"

// Everything that gets drawn: the sky dome, the static objects and the
// player, built from a .scene file (see SceneFile and docs/ARCHITECTURE.md).
// Owns the models; each model file is loaded once and shared by every object
// that uses it. Needs a current OpenGL context.
class Scene {
private:
  SceneFile file;
  std::map<std::string, std::unique_ptr<Model>> models;
  std::unique_ptr<GameObject> sky;
  std::vector<GameObject> objects;
  std::vector<Effect> effects; // effects[i] belongs to objects[i]
  std::unique_ptr<AnimatedModel> playerModel;
  std::unique_ptr<GameObject> player;
  bool playerVisible = true;

  Model *getModel(const std::string &path);

public:
  // Parses `scenePath` and loads its models; model paths in the file are
  // relative to `assetDir`. Returns false if the file could not be parsed.
  bool load(const std::string &scenePath, const std::string &assetDir);

  const SceneFile &info() const { return file; }
  // The controllable character, or nullptr if the scene has none.
  GameObject *getPlayer() { return player.get(); }
  // Hide the player in first person, or the camera ends up inside its head
  void setPlayerVisible(bool visible) { playerVisible = visible; }

  // Advances the player's animation to `seconds`.
  void Update(double seconds);
  // Draws the whole scene. The sky is centred on `cameraPosition`; `time`
  // drives the star twinkle and the breathing.
  void Draw(Shader *shader, glm::vec3 cameraPosition, float time);
};

#endif
