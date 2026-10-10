#include "MapList.h"

#include "ForestStage.h"
#include "MapEdits.h"
#include "PineForestStage.h"
#include "PlayerHouseStage.h"
#include "Route66Stage.h"
#include "SceneStage.h"
#include "TestStage.h"

static bool editsApplied = true;
void setMapEditsApplied(bool applied) { editsApplied = applied; }

const std::vector<MapEntry> &mapList() {
  static const std::vector<MapEntry> built = {
      {"Desierto de dia",
       [](const MapContext &c) {
         return std::unique_ptr<GameStage>(new TestStage(c.floorMode, c.sound, c.speech));
       }},
      {"Bosque",
       [](const MapContext &c) -> std::unique_ptr<GameStage> {
         std::unique_ptr<ForestStage> forest(new ForestStage(c.floorMode, c.sound));
         if (!forest->isValid())
           return nullptr;
         return std::unique_ptr<GameStage>(forest.release());
       }},
      {"Ruta 66",
       [](const MapContext &c) -> std::unique_ptr<GameStage> {
         std::unique_ptr<Route66Stage> route(new Route66Stage(c.floorMode, c.sound, c.speech));
         if (!route->isValid())
           return nullptr;
         return std::unique_ptr<GameStage>(route.release());
       }},
      {"Desierto de noche",
       [](const MapContext &c) -> std::unique_ptr<GameStage> {
         return SceneStage::load("../assets/scenes/desert.scene", "../assets", c.floorMode);
       }},
      {"Bosque de pinos",
       [](const MapContext &c) {
         return std::unique_ptr<GameStage>(new PineForestStage(c.floorMode, c.sound));
       }},
      {"player_house",
       [](const MapContext &c) {
         return std::unique_ptr<GameStage>(new PlayerHouseStage(c.floorMode, c.sound));
       }},
  };
  // Each map, with the map editor's changes over it
  static const std::vector<MapEntry> maps = [&]() {
    std::vector<MapEntry> list;
    for (const MapEntry &entry : built) {
      MapEntry wrapped;
      wrapped.name = entry.name;
      auto create = entry.create;
      std::string name = entry.name;
      wrapped.create = [create, name](const MapContext &c) {
        std::unique_ptr<GameStage> stage = create(c);
        if (stage && editsApplied) {
          EntityContext context = {c.sound, c.speech};
          applyMapEdits(*stage, name, context);
        }
        return stage;
      };
      list.push_back(wrapped);
    }
    return list;
  }();
  return maps;
}
