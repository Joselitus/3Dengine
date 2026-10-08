#include "MapList.h"

#include "ForestStage.h"
#include "PineForestStage.h"
#include "Route66Stage.h"
#include "SceneStage.h"
#include "TestStage.h"

const std::vector<MapEntry> &mapList() {
  static const std::vector<MapEntry> maps = {
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
  };
  return maps;
}
