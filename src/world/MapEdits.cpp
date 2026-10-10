#include "MapEdits.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "GameStage.h"
#include "PropCatalog.h"

using namespace std;
using namespace glm;

string MapEdits::pathFor(const string &mapName) {
  string slug;
  for (char c : mapName) {
    if (isalnum((unsigned char)c))
      slug += (char)tolower((unsigned char)c);
    else if (!slug.empty() && slug.back() != '_')
      slug += '_';
  }
  while (!slug.empty() && slug.back() == '_')
    slug.pop_back();
  return "../assets/edits/" + slug + ".edit";
}

bool MapEdits::load(const string &path) {
  ifstream in(path);
  if (!in)
    return false;
  string line;
  int number = 0;
  while (getline(in, line)) {
    number++;
    if (line.empty() || line[0] == '#')
      continue;
    istringstream fields(line);
    string command;
    fields >> command;
    bool ok = true;
    if (command == "grid") {
      ok = (bool)(fields >> gridX >> gridZ);
    } else if (command == "height") {
      int ix, iz;
      float delta;
      ok = (bool)(fields >> ix >> iz >> delta) && gridX > 0;
      if (ok)
        heights[(unsigned int)(iz * gridX + ix)] = delta;
    } else if (command == "material") {
      int cx, cz, m;
      ok = (bool)(fields >> cx >> cz >> m) && gridX > 1;
      if (ok)
        materials[(unsigned int)(cz * (gridX - 1) + cx)] = (unsigned char)m;
    } else if (command == "prop") {
      Prop p;
      ok = (bool)(fields >> p.type >> p.x >> p.z >> p.above >> p.yaw >> p.scale);
      if (ok)
        props.push_back(p);
    } else if (command == "entity") {
      Entity e;
      ok = (bool)(fields >> e.kind >> e.x >> e.z >> e.yaw);
      if (ok)
        entities.push_back(e);
    } else if (command == "road") {
      Road r;
      int closed = 0, count = 0;
      ok = (bool)(fields >> r.type >> r.width >> closed >> count) && count >= 0 && count < 100000;
      for (int i = 0; ok && i < count; i++) {
        vec2 p;
        ok = (bool)(fields >> p.x >> p.y);
        r.points.push_back(p);
      }
      r.closed = closed != 0;
      if (ok)
        roads.push_back(r);
    } else if (command == "move" || command == "delete") {
      Move m;
      char kind;
      ok = (bool)(fields >> kind >> m.index);
      m.dynamic = kind == 'd';
      m.deleted = command == "delete";
      m.position = vec3(0.0f);
      m.yaw = 0.0f;
      if (ok && !m.deleted)
        ok = (bool)(fields >> m.position.x >> m.position.y >> m.position.z >> m.yaw);
      if (ok)
        moves.push_back(m);
    } else {
      ok = false;
    }
    if (!ok)
      fprintf(stderr, "%s:%d: cannot read '%s'\n", path.c_str(), number, line.c_str());
  }
  return true;
}

bool MapEdits::save(const string &path) const {
  string temporary = path + ".tmp";
  {
    ofstream out(temporary);
    if (!out)
      return false;
    out << "# Edits of a map (MapEdits, made by the map editor): a layer over the map the game builds.\n";
    if (gridX > 0)
      out << "grid " << gridX << " " << gridZ << "\n";
    char buffer[256];
    for (const auto &h : heights) {
      snprintf(buffer, sizeof(buffer), "height %d %d %.4g\n", (int)(h.first % gridX), (int)(h.first / gridX), h.second);
      out << buffer;
    }
    for (const auto &m : materials) {
      snprintf(buffer, sizeof(buffer), "material %d %d %d\n", (int)(m.first % (gridX - 1)), (int)(m.first / (gridX - 1)),
               (int)m.second);
      out << buffer;
    }
    for (const Prop &p : props) {
      snprintf(buffer, sizeof(buffer), "prop %s %.4f %.4f %.4f %.4f %.4f\n", p.type.c_str(), p.x, p.z, p.above, p.yaw,
               p.scale);
      out << buffer;
    }
    for (const Entity &e : entities) {
      snprintf(buffer, sizeof(buffer), "entity %s %.4f %.4f %.4f\n", e.kind.c_str(), e.x, e.z, e.yaw);
      out << buffer;
    }
    for (const Road &r : roads) {
      out << "road " << r.type << " " << r.width << " " << (r.closed ? 1 : 0) << " " << r.points.size();
      for (const vec2 &p : r.points) {
        snprintf(buffer, sizeof(buffer), " %.3f %.3f", p.x, p.y);
        out << buffer;
      }
      out << "\n";
    }
    for (const Move &m : moves) {
      if (m.deleted)
        snprintf(buffer, sizeof(buffer), "delete %c %d\n", m.dynamic ? 'd' : 's', m.index);
      else
        snprintf(buffer, sizeof(buffer), "move %c %d %.4f %.4f %.4f %.4f\n", m.dynamic ? 'd' : 's', m.index,
                 m.position.x, m.position.y, m.position.z, m.yaw);
      out << buffer;
    }
    if (!out)
      return false;
  }
  return rename(temporary.c_str(), path.c_str()) == 0;
}

MapEdits::Move &MapEdits::moveOf(bool dynamic, int index) {
  for (Move &m : moves)
    if (m.dynamic == dynamic && m.index == index)
      return m;
  Move m;
  m.dynamic = dynamic;
  m.index = index;
  m.position = vec3(0.0f);
  m.yaw = 0.0f;
  m.deleted = false;
  moves.push_back(m);
  return moves.back();
}

void MapEdits::apply(GameStage &stage, EntityContext &context) {
  baseStatics = stage.getObjects().size();
  baseDynamics = stage.getDynamicObjects().size();

  // --- Terrain. What stands on the ground (the solid scenery) keeps its height over it
  bool terrain = (!heights.empty() || !materials.empty()) && stage.terrainEditable();
  if (terrain) {
    Stage::TerrainGrid g = stage.terrainGrid();
    if (g.nx != gridX || g.nz != gridZ) {
      fprintf(stderr, "MapEdits: the terrain has %d x %d points, the edits are for %d x %d: terrain ignored\n", g.nx,
              g.nz, gridX, gridZ);
      terrain = false;
    }
  }
  if (terrain) {
    vector<pair<GameObject *, float>> standing; // the object and its height over the old ground
    for (const auto &o : stage.getObjects()) {
      float ground;
      if (!o->isCollidable() || !stage.floorAt(o->getPosition().x, o->getPosition().z, ground))
        continue;
      float over = o->getPosition().y - ground;
      if (std::fabs(over) < 3.0f)
        standing.push_back({o.get(), over});
    }
    for (const auto &h : heights) {
      int ix = (int)(h.first % gridX), iz = (int)(h.first / gridX);
      stage.setTerrainHeight(ix, iz, stage.terrainHeight(ix, iz) + h.second);
    }
    for (const auto &m : materials)
      stage.setTerrainMaterial((int)(m.first % (gridX - 1)), (int)(m.first / (gridX - 1)), (FloorMaterial)m.second);
    stage.commitTerrain();
    for (const auto &s : standing) {
      vec3 at = s.first->getPosition();
      float ground;
      if (stage.floorAt(at.x, at.z, ground) && std::fabs(ground + s.second - at.y) > 1e-4f)
        s.first->teleport(vec3(at.x, ground + s.second, at.z));
    }
    stage.refreshStaticGrid();
  }

  // --- The map's own objects, moved or taken away
  for (const Move &m : moves) {
    GameObject *object = nullptr;
    if (m.dynamic) {
      if (m.index >= 0 && m.index < (int)baseDynamics)
        object = stage.getDynamicObjects()[m.index].get();
    } else if (m.index >= 0 && m.index < (int)baseStatics) {
      object = stage.getObjects()[m.index].get();
    }
    if (!object)
      continue;
    if (m.deleted) {
      object->setVisible(false);
      object->setCollidable(false);
      continue;
    }
    // (its height is over the floor, so that it follows the terrain if that is changed)
    vec3 at = m.position;
    float ground;
    if (stage.floorAt(at.x, at.z, ground))
      at.y += ground;
    stage.relocate(*object, at);
    stage.turn(*object, std::remainder(m.yaw - object->getHeading(), 2.0f * pi<float>()));
  }

  // --- New objects
  propObjects.clear();
  for (const Prop &p : props) {
    const PropType *type = findProp(p.type);
    propObjects.push_back(type ? makeProp(stage, *type, p.x, p.z, p.above, p.yaw, p.scale) : nullptr);
    if (!type)
      fprintf(stderr, "MapEdits: no prop called '%s'\n", p.type.c_str());
  }
  entityObjects.clear();
  for (const Entity &e : entities) {
    size_t before = stage.getDynamicObjects().size();
    float ground = 0.0f;
    stage.floorAt(e.x, e.z, ground);
    bool made = stage.spawnEntity(e.kind, vec3(e.x, ground, e.z), e.yaw, context);
    if (made && stage.getDynamicObjects().size() > before)
      entityObjects.push_back(stage.getDynamicObjects().back());
    else
      entityObjects.push_back(nullptr);
    if (!made)
      fprintf(stderr, "MapEdits: this map cannot have a '%s'\n", e.kind.c_str());
  }

  // --- Roads (on the terrain as it is after the edits)
  roadMeshes.clear();
  rebuildRoads(stage);
}

void MapEdits::rebuildRoads(GameStage &stage) {
  stage.roadSet().set(roads);
  while (roadMeshes.size() < roads.size())
    roadMeshes.push_back(make_shared<RoadMesh>(stage));
  for (size_t i = 0; i < roadMeshes.size(); i++) {
    if (i < roads.size())
      roadMeshes[i]->update(stage, roads[i], (int)i);
    else
      roadMeshes[i]->setVisible(false);
  }
}

void applyMapEdits(GameStage &stage, const string &mapName, EntityContext &context) {
  MapEdits edits;
  if (!edits.load(MapEdits::pathFor(mapName)))
    return;
  edits.apply(stage, context);
  fprintf(stderr, "Map edits applied to '%s': %zu props, %zu creatures, %zu roads, %zu objects moved, %zu terrain points\n",
          mapName.c_str(), edits.props.size(), edits.entities.size(), edits.roads.size(), edits.moves.size(),
          edits.heights.size());
}
