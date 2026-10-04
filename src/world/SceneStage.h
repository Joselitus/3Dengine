#ifndef SCENE_STAGE
#define SCENE_STAGE

#include <memory>
#include <string>

#include "GameStage.h"
#include "SceneFile.h"

// A map described by a .scene file (see SceneFile and docs/ARCHITECTURE.md),
// e.g. assets/scenes/desert.scene, the desert at night. The same file is
// shown by the web viewer (tools/scene_viewer).
//
//   floor   -> the stage's floor, also drawn
//   object  -> a GameObject; "ground" as y stands it on the floor (sunk
//              PROP_SINK so it doesn't float on slopes); effects: emissive
//              draws it unlit = 2, breathe sets its breath amplitude
//   player  -> a Walker with that animated model, under gravity
//   camera, moon, light, fog, sky -> camera, environment and sky dome
class SceneStage : public GameStage {
private:
  SceneStage(const SceneFile &file, const std::string &assetDir,
             FloorMode mode);

public:
  static constexpr float PROP_SINK = 0.05f;
  static constexpr float BREATH_AMPLITUDE = 2.0f;

  // nullptr if the file can't be read or parsed (the error is printed).
  // Model paths in the file are relative to `assetDir`.
  static std::unique_ptr<SceneStage> load(const std::string &path,
                                          const std::string &assetDir,
                                          FloorMode mode);
};

#endif
