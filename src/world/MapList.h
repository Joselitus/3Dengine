#ifndef MAP_LIST
#define MAP_LIST

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "GameStage.h"
#include "SoundEngine.h"
#include "SpeechSynthesizer.h"

// What a map needs to be made (the same on the server and on the clients)
struct MapContext {
  FloorMode floorMode;
  SoundEngine &sound;
  SpeechSynthesizer &speech;
};

// One of the game's maps. The server and every client build the map from the same list, so that a
// map's index means the same thing on both sides (and their objects come out in the same order:
// see Stage::getNetId).
struct MapEntry {
  std::string name;
  // nullptr if the map could not be loaded
  std::function<std::unique_ptr<GameStage>(const MapContext &)> create;
};

// The maps, in the order the map selector lists them
const std::vector<MapEntry> &mapList();

#endif
