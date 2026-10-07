#include "SceneFile.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

static bool parseEffect(const string &name, Effect &effect) {
  if (name == "lit")
    effect = Effect::Lit;
  else if (name == "emissive")
    effect = Effect::Emissive;
  else if (name == "breathe")
    effect = Effect::Breathe;
  else if (name == "sway")
    effect = Effect::Sway;
  else
    return false;
  return true;
}

// A height: a number, or "ground" (stand on the floor, see SceneStage)
static bool parseHeight(istringstream &fields, float &y, bool &onGround) {
  string word;
  if (!(fields >> word))
    return false;
  onGround = word == "ground";
  if (onGround) {
    y = 0.0f;
    return true;
  }
  istringstream number(word);
  return bool(number >> y) && number.eof();
}

bool SceneFile::load(const string &path) {
  ifstream in(path);
  if (!in) {
    cerr << path << ": cannot open scene file" << endl;
    return false;
  }

  string line;
  int lineNumber = 0;
  while (getline(in, line)) {
    lineNumber++;
    size_t comment = line.find('#');
    if (comment != string::npos)
      line.erase(comment);

    istringstream fields(line);
    string command;
    if (!(fields >> command))
      continue; // blank line

    bool ok;
    if (command == "moon") {
      ok = bool(fields >> moonDir.x >> moonDir.y >> moonDir.z);
      if (ok)
        moonDir = glm::normalize(moonDir);
    } else if (command == "light")
      ok = bool(fields >> lightColor.r >> lightColor.g >> lightColor.b);
    else if (command == "fog")
      ok = bool(fields >> fogColor.r >> fogColor.g >> fogColor.b);
    else if (command == "time_of_day")
      ok = bool(fields >> timeOfDay);
    else if (command == "day_duration")
      ok = bool(fields >> dayDuration);
    else if (command == "sky")
      ok = bool(fields >> sky);
    else if (command == "procedural_sky")
      ok = bool(fields >> proceduralSky) &&
           (proceduralSky == "dunes" || proceduralSky == "forest");

    else if (command == "floor")
      ok = bool(fields >> floor >> floorPosition.x >> floorPosition.y >>
                floorPosition.z);
    else if (command == "player")
      ok = bool(fields >> player >> playerPosition.x) &&
           parseHeight(fields, playerPosition.y, playerOnGround) &&
           bool(fields >> playerPosition.z);
    else if (command == "camera")
      ok = bool(fields >> cameraDistance >> cameraHeight);
    else if (command == "object") {
      SceneObject o;
      o.effect = Effect::Lit;
      ok = bool(fields >> o.model >> o.position.x) &&
           parseHeight(fields, o.position.y, o.onGround) &&
           bool(fields >> o.position.z >> o.yaw >> o.scale);
      string effect;
      if (ok && fields >> effect && !parseEffect(effect, o.effect)) {
        cerr << path << ":" << lineNumber << ": unknown effect '" << effect
             << "'" << endl;
        return false;
      }
      if (ok)
        objects.push_back(o);
    } else {
      cerr << path << ":" << lineNumber << ": unknown command '" << command
           << "'" << endl;
      return false;
    }

    if (!ok) {
      cerr << path << ":" << lineNumber << ": wrong arguments for '"
           << command << "'" << endl;
      return false;
    }
  }
  return true;
}
