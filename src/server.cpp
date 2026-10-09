// The game's server: it runs the world (the map, its creatures, Bob, the RV, the day) with no
// window and no sound, lets clients in (client.cpp) and tells them what happens. See net/.
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include <glm/glm.hpp>

#include "Gfx.h"
#include "MapList.h"
#include "NetRole.h"
#include "NetServer.h"
#include "Paths.h"
#include "SilentSynthesizer.h"
#include "SoundEngine.h"

using namespace std;

static volatile sig_atomic_t running = 1;
static void stop(int) { running = 0; }

static void usage(const char *program) {
  printf("Usage: %s [--port N] [--map N|NAME] [--time H] [--day-duration S] [--ray] [--list]\n"
         "  --port N           TCP port (default %d)\n"
         "  --map N|NAME       map to serve (number in the list, or name; --list shows them)\n"
         "  --time H           hour of the day the map starts at\n"
         "  --day-duration S   real seconds a day lasts (0 = stopped)\n"
         "  --ray              floor by ray casting (default: height map)\n"
         "  --verbose          every two seconds prints where each player is\n",
         program, Net::DEFAULT_PORT);
}

int main(int argc, char **argv) {
  FloorMode floorMode = FloorMode::HeightField;
  float startHour = -1.0f, dayDuration = -1.0f;
  string startMap = "0";
  bool verbose = false;
  int port = Net::DEFAULT_PORT;
  const auto &maps = mapList();
  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    if (arg == "--port" && i + 1 < argc)
      port = atoi(argv[++i]);
    else if (arg == "--map" && i + 1 < argc)
      startMap = argv[++i];
    else if (arg == "--time" && i + 1 < argc)
      startHour = (float)atof(argv[++i]);
    else if (arg == "--day-duration" && i + 1 < argc)
      dayDuration = (float)atof(argv[++i]);
    else if (arg == "--ray")
      floorMode = FloorMode::DownwardRay;
    else if (arg == "--verbose")
      verbose = true;
    else if (arg == "--list") {
      for (size_t m = 0; m < maps.size(); m++)
        printf("%zu  %s\n", m, maps[m].name.c_str());
      return 0;
    } else {
      usage(argv[0]);
      return arg == "--help" || arg == "-h" ? 0 : 1;
    }
  }
  int mapIndex = -1;
  for (size_t i = 0; i < maps.size(); i++)
    if (startMap == maps[i].name || startMap == to_string(i))
      mapIndex = (int)i;
  if (mapIndex < 0) {
    fprintf(stderr, "No map '%s' (--list shows them)\n", startMap.c_str());
    return 1;
  }
  if (!enterSourceDir())
    fprintf(stderr, "Could not find src/, using the current directory\n");

  setvbuf(stdout, nullptr, _IOLBF, 0); // (the log shows as it happens, even in a file)
  setNetRole(NetRole::Server);
  Gfx::headless = true; // no window, no graphics: the world is only simulated
  signal(SIGINT, stop);
  signal(SIGTERM, stop);

  SoundEngine sound(true);
  SilentSynthesizer speech;
  MapContext context = {floorMode, sound, speech};
  printf("[server] loading map '%s'...\n", maps[mapIndex].name.c_str());
  fflush(stdout);
  unique_ptr<GameStage> stage = maps[mapIndex].create(context);
  if (!stage) {
    fprintf(stderr, "Could not load map '%s'\n", maps[mapIndex].name.c_str());
    return 1;
  }
  if (startHour >= 0.0f)
    stage->setTimeOfDay(startHour);
  if (dayDuration >= 0.0f)
    stage->setDayDuration(dayDuration);

  NetServer net(*stage, mapIndex, maps[mapIndex].name);
  string error;
  if (!net.start(port, error)) {
    fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }
  printf("[server] ready: map '%s' on port %d\n", maps[mapIndex].name.c_str(), port);
  fflush(stdout);

  // The world moves in fixed steps; the network is attended to in between
  using clock = chrono::steady_clock;
  auto last = clock::now();
  double owed = 0.0;
  unsigned ticks = 0;
  while (running) {
    auto now = clock::now();
    double dt = chrono::duration<double>(now - last).count();
    last = now;
    owed = min(owed + dt, 0.25); // (after a long stall it does not rush to catch up)
    net.poll(dt);
    if (net.resetWanted()) {
      // /reset: the map is made again (the players have to come back in)
      printf("[server] starting map '%s' again...\n", maps[mapIndex].name.c_str());
      fflush(stdout);
      unique_ptr<GameStage> fresh = maps[mapIndex].create(context);
      if (fresh) {
        if (startHour >= 0.0f)
          fresh->setTimeOfDay(startHour);
        if (dayDuration >= 0.0f)
          fresh->setDayDuration(dayDuration);
        net.restart(*fresh);
        stage = std::move(fresh);
      } else {
        net.cancelReset();
        fprintf(stderr, "[server] could not make the map again\n");
      }
      last = clock::now();
      owed = 0.0;
      continue;
    }
    while (owed >= Net::TICK) {
      stage->tick(Net::TICK);
      owed -= Net::TICK;
      if (++ticks % Net::TICKS_PER_SNAPSHOT == 0)
        net.sendSnapshots();
      if (verbose && ticks % 120 == 0)
        for (const auto &p : stage->getPlayers()) {
          glm::vec3 at = p->getPosition();
          printf("[state] %s: (%.2f, %.2f, %.2f)%s%s%s\n", p->name.c_str(), at.x, at.y, at.z,
                 p->dead ? " dead" : "", p->inVehicle ? " in the RV" : "", p->inSaucer ? " in the ship" : "");
        }
    }
    this_thread::sleep_for(chrono::milliseconds(1));
  }
  printf("[server] bye\n");
  return 0;
}
