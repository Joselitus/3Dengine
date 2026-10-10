// The map editor: opens one of the game's maps and lets you put things in it and reshape its
// terrain. What you change is saved as a layer over the map (assets/edits/<map>.edit, see MapEdits),
// which the game applies to the map when it builds it, so the map's code and generators stay as they
// are.
//   ../test/map_editor [--map N|NAME] [--time H] [--list]
// Keys: right button + mouse looks around, W A S D fly, Q / E (or C / Space) down / up, Shift faster, wheel (with
// the right button) changes the speed; 1 select, 2 prop, 3 creature, 4 raise, 5 lower, 6 smooth,
// 7 flatten, 8 paint, 9 road; wheel turns the thing being placed or selected (Shift: its size) or changes the
// brush radius; Delete removes the selected one; Ctrl+S saves; Ctrl+Z undoes; Esc quits.
// Roads (9): a road is a spline through control points. Click on the ground adds a point at the end
// of the road being drawn (or starts one), clicking a point selects and drags it, Shift+click on the
// road puts a new point there, Delete takes the selected point away.
#include <GL/glew.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>

#include "Camera.h"
#include "Gfx.h"
#include "Light.h"
#include "LineRenderer.h"
#include "MapEdits.h"
#include "MapList.h"
#include "Paths.h"
#include "PropCatalog.h"
#include "Shader.h"
#include "SilentSynthesizer.h"
#include "SoundEngine.h"
#include "UIButton.h"
#include "UILabel.h"
#include "UIManager.h"
#include "UISlider.h"

using namespace std;
using namespace glm;

namespace {

const int WIDTH = 1280, HEIGHT = 800;
const float TOOL_PANEL_WIDTH = 300.0f, CATALOG_WIDTH = 290.0f, MARGIN = 10.0f;
const float PICK_RANGE = 1500.0f;
const float STANDING_RANGE = 3.0f;        // an object this close to the ground stands on it
const vec4 SELECT_COLOR(1.0f, 0.6f, 0.15f, 1.0f), HOVER_COLOR(0.5f, 0.8f, 1.0f, 0.9f);

enum class Tool { Select, Prop, Creature, Raise, Lower, Smooth, Flatten, Paint, Road };
const int TOOL_COUNT = 9;
const char *TOOL_NAMES[] = {"1  Seleccionar / mover", "2  Colocar objeto", "3  Colocar criatura", "4  Elevar terreno",
                            "5  Bajar terreno",       "6  Suavizar",       "7  Aplanar",          "8  Pintar suelo",
                            "9  Carreteras / caminos"};
const char *TOOL_HELP[] = {
    "Clic: elegir; arrastrar: mover; rueda: girar (Mayus: tamano); Supr: borrar",
    "Clic: poner el objeto elegido; rueda: girar (Mayus: tamano)",
    "Clic: poner la criatura elegida; rueda: girar",
    "Mantener clic: elevar; rueda: radio del pincel",
    "Mantener clic: bajar; rueda: radio del pincel",
    "Mantener clic: suavizar; rueda: radio del pincel",
    "Mantener clic: llevar el terreno a la altura donde empiezas; rueda: radio",
    "Mantener clic: pintar el tipo de suelo elegido; rueda: radio",
    "Clic: anadir punto (o elegir/arrastrar uno); Mayus+clic en la curva: punto nuevo; Supr: quitar punto"};
const float POINT_PICK = 14.0f; // pixels: how near the cursor must be to a control point

float scrollSum = 0.0f;
void onScroll(GLFWwindow *, double, double dy) { scrollSum += (float)dy; }

GLFWwindow *openWindow() {
  glewExperimental = true;
#if defined(GLFW_PLATFORM) && !defined(__APPLE__) // macOS has no X11
  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return nullptr;
  }
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "map editor", NULL, NULL);
  if (!window) {
    fprintf(stderr, "Failed to open the window\n");
    glfwTerminate();
    return nullptr;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "Failed to initialize GLEW\n");
    return nullptr;
  }
  return window;
}

// A ray from the camera through a point of the window
void cursorRay(GLFWwindow *window, Camera &camera, double x, double y, vec3 &origin, vec3 &dir) {
  int w, h;
  glfwGetWindowSize(window, &w, &h);
  float nx = 2.0f * (float)x / w - 1.0f, ny = 1.0f - 2.0f * (float)y / h;
  mat4 inv = inverse(camera.getViewProjection());
  vec4 a = inv * vec4(nx, ny, -1.0f, 1.0f), b = inv * vec4(nx, ny, 1.0f, 1.0f);
  origin = vec3(a) / a.w;
  dir = normalize(vec3(b) / b.w - origin);
}

// Where the ray first meets the terrain (marching, then halving)
bool terrainRay(const Stage &stage, const vec3 &origin, const vec3 &dir, vec3 &hit) {
  float previous = 0.0f, height;
  bool wasAbove = true;
  for (float t = 0.0f; t < PICK_RANGE; t += 0.5f + t * 0.002f) {
    vec3 p = origin + dir * t;
    bool above = !stage.floorAt(p.x, p.z, height) || p.y > height;
    if (!above && wasAbove && t > 0.0f) {
      float lo = previous, hi = t;
      for (int i = 0; i < 10; i++) {
        float mid = 0.5f * (lo + hi);
        vec3 q = origin + dir * mid;
        if (stage.floorAt(q.x, q.z, height) && q.y <= height)
          hi = mid;
        else
          lo = mid;
      }
      hit = origin + dir * hi;
      stage.floorAt(hit.x, hit.z, hit.y);
      return true;
    }
    wasAbove = above;
    previous = t;
  }
  return false;
}

string lower(string s) {
  for (char &c : s)
    c = (char)tolower((unsigned char)c);
  return s;
}

// A window that keeps every key to itself (the editor reads the keyboard on its own)
class EditorPanel : public UIPanel {
public:
  EditorPanel(const string &title, float width) : UIPanel(title, width, false) {}
  bool onKey(int) override { return true; }
};

// What is selected: an object of the map, or one the editor added, and which one in its list
struct Selection {
  shared_ptr<GameObject> object;
  enum Kind { None, BaseStatic, BaseDynamic, AddedProp, AddedEntity } kind = None;
  int index = -1;
};

} // namespace

int main(int argc, char **argv) {
  string mapArg = "0";
  float startHour = 12.0f;
  bool list = false;
  const auto &maps = mapList();
  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    if (arg == "--map" && i + 1 < argc)
      mapArg = argv[++i];
    else if (arg == "--time" && i + 1 < argc)
      startHour = (float)atof(argv[++i]);
    else if (arg == "--list")
      list = true;
    else {
      fprintf(stderr, "Usage: %s [--map N|NAME] [--time H] [--list]\n", argv[0]);
      return arg == "--help" || arg == "-h" ? 0 : 1;
    }
  }
  if (list) {
    for (size_t m = 0; m < maps.size(); m++)
      printf("%zu  %s\n", m, maps[m].name.c_str());
    return 0;
  }
  int mapIndex = -1;
  for (size_t i = 0; i < maps.size(); i++)
    if (mapArg == to_string(i) || lower(maps[i].name).find(lower(mapArg)) != string::npos)
      mapIndex = (int)i;
  if (mapIndex < 0) {
    fprintf(stderr, "No map '%s' (--list shows them)\n", mapArg.c_str());
    return 1;
  }
  if (!enterSourceDir())
    fprintf(stderr, "Could not find src/, using the current directory\n");
  GLFWwindow *window = openWindow();
  if (!window)
    return -1;
  glEnable(GL_DEPTH_TEST);
  glfwSetScrollCallback(window, onScroll);
  setMapEditsApplied(false); // the editor applies the layer itself, to keep working on it

  Shader shader("shaders/animatedshader.vert", "shaders/shader.frag");
  Camera camera(window, &shader);
  Light light(1.0f, 1.0f, 1.0f, &shader);
  shader.setInt("unlit", 0);
  UIManager ui(window);
  LineRenderer lines;
  SoundEngine sound(true);
  SilentSynthesizer speech;
  MapContext context = {FloorMode::HeightField, sound, speech};
  EntityContext entityContext = {sound, speech};

  // --- The map being edited
  unique_ptr<GameStage> stage;
  MapEdits edits;
  vector<float> baseHeights;
  vector<unsigned char> baseMaterials;
  Stage::TerrainGrid grid;
  string status;
  bool modified = false;
  Selection selected, hovered;
  vector<function<void()>> undoStack; // see "Undo" below

  auto loadMap = [&](int index) -> bool {
    selected = Selection();
    hovered = Selection();
    undoStack.clear();
    stage.reset();
    printf("Loading '%s'...\n", maps[index].name.c_str());
    fflush(stdout);
    stage = maps[index].create(context);
    if (!stage) {
      status = "No puedo cargar el mapa " + maps[index].name;
      return false;
    }
    mapIndex = index;
    stage->setDayDuration(0.0f);
    stage->setTimeOfDay(startHour);
    grid = stage->terrainGrid();
    baseHeights.clear();
    baseMaterials.clear();
    if (stage->terrainEditable()) {
      for (int iz = 0; iz < grid.nz; iz++)
        for (int ix = 0; ix < grid.nx; ix++)
          baseHeights.push_back(stage->terrainHeight(ix, iz));
      for (int cz = 0; cz < grid.nz - 1; cz++)
        for (int cx = 0; cx < grid.nx - 1; cx++)
          baseMaterials.push_back((unsigned char)stage->terrainMaterial(cx, cz));
    }
    edits = MapEdits();
    edits.load(MapEdits::pathFor(maps[index].name));
    edits.apply(*stage, entityContext);
    edits.gridX = grid.nx;
    edits.gridZ = grid.nz;
    modified = false;
    camera.setFarPlane(std::max(stage->getFarPlane(), 1000.0f));
    status = "Mapa: " + maps[index].name;
    return true;
  };
  if (!loadMap(mapIndex))
    return 1;

  // --- The flying camera
  float yaw = 0.0f, pitch = 0.5f, flySpeed = 25.0f;
  vec3 eye(0.0f);
  auto lookAtStart = [&]() {
    vec3 start = stage->getSpawnPoint();
    eye = start + vec3(0.0f, 14.0f, 22.0f);
    yaw = 0.0f;
    pitch = 0.5f;
  };
  lookAtStart();

  // --- The tools
  Tool tool = Tool::Select;
  float brushRadius = 8.0f, brushStrength = 6.0f, placeYaw = 0.0f, placeScale = 1.0f, hour = startHour;
  bool randomize = true;
  int propIndex = 0, entityIndex = 0;
  FloorMaterial paintMaterial = FloorMaterial::Grass;
  int roadType = Road::Asphalt, roadSelected = -1, pointSelected = -1, pointHovered = -1, roadHovered = -1;
  float roadWidth = 8.0f;
  bool pointDragging = false;
  string propCategory = "Desierto";
  EditorPanel *toolPanel = nullptr, *catalogPanel = nullptr;
  bool rebuildCatalog = true, openMapList = false;
  int requestedMap = -1;
  bool quitArmed = false;

  auto save = [&]() {
    edits.gridX = grid.nx;
    edits.gridZ = grid.nz;
    edits.heights.clear();
    edits.materials.clear();
    if (stage->terrainEditable()) {
      for (int iz = 0; iz < grid.nz; iz++)
        for (int ix = 0; ix < grid.nx; ix++) {
          size_t i = (size_t)iz * grid.nx + ix;
          float d = stage->terrainHeight(ix, iz) - baseHeights[i];
          if (std::fabs(d) > 0.002f)
            edits.heights[(unsigned int)i] = d;
        }
      for (int cz = 0; cz < grid.nz - 1; cz++)
        for (int cx = 0; cx < grid.nx - 1; cx++) {
          size_t i = (size_t)cz * (grid.nx - 1) + cx;
          unsigned char m = (unsigned char)stage->terrainMaterial(cx, cz);
          if (m != baseMaterials[i])
            edits.materials[(unsigned int)i] = m;
        }
    }
    mkdir("../assets/edits", 0755);
    string path = MapEdits::pathFor(maps[mapIndex].name);
    if (edits.save(path)) {
      modified = false;
      char text[300];
      snprintf(text, sizeof(text), "Guardado en %s (%zu objetos, %zu criaturas, %zu movidos, %zu puntos de terreno)",
               path.c_str(), edits.props.size(), edits.entities.size(), edits.moves.size(), edits.heights.size());
      status = text;
    } else {
      status = "No he podido guardar " + path;
    }
    printf("%s\n", status.c_str());
  };

  // --- Roads: undo is a copy of the list of roads and what is selected in it
  auto pushRoadUndo = [&]() {
    vector<Road> before = edits.roads;
    int roadBefore = roadSelected, pointBefore = pointSelected;
    undoStack.push_back([&, before, roadBefore, pointBefore]() {
      edits.roads = before;
      roadSelected = roadBefore;
      pointSelected = pointBefore;
      edits.rebuildRoads(*stage);
    });
    if (undoStack.size() > 200)
      undoStack.erase(undoStack.begin());
  };
  // Takes the selected control point away (the road with it if it was the last)
  auto removeRoadPoint = [&]() {
    if (roadSelected < 0 || roadSelected >= (int)edits.roads.size() || pointSelected < 0 ||
        pointSelected >= (int)edits.roads[roadSelected].points.size()) {
      status = "Elige un punto de una carretera";
      return;
    }
    pushRoadUndo();
    Road &road = edits.roads[roadSelected];
    road.points.erase(road.points.begin() + pointSelected);
    pointSelected = -1;
    if (road.points.empty()) {
      edits.roads.erase(edits.roads.begin() + roadSelected);
      roadSelected = -1;
    }
    edits.rebuildRoads(*stage);
    modified = true;
    status = "Punto borrado";
  };

  // The catalog panel changes with the tool (the objects, the creatures, the kinds of ground)
  auto buildCatalog = [&]() {
    if (catalogPanel)
      ui.close(catalogPanel);
    catalogPanel = nullptr;
    if (tool != Tool::Prop && tool != Tool::Creature && tool != Tool::Paint && tool != Tool::Road)
      return;
    catalogPanel = new EditorPanel(tool == Tool::Prop       ? "Objetos"
                                   : tool == Tool::Creature ? "Criaturas"
                                   : tool == Tool::Road     ? "Carreteras"
                                                            : "Tipo de suelo",
                                   CATALOG_WIDTH);
    if (tool == Tool::Road) {
      const char *kinds[] = {"Asfalto", "Camino de tierra"};
      for (int k = 0; k < 2; k++) {
        int type = k;
        string name = kinds[k];
        catalogPanel->add(new UIButton([&, type, name]() { return (roadType == type ? "> " : "  ") + name; },
                                       [&, type]() {
                                         roadType = type;
                                         roadWidth = type == Road::Dirt ? 5.0f : 8.0f;
                                         if (roadSelected >= 0 && roadSelected < (int)edits.roads.size()) {
                                           pushRoadUndo();
                                           edits.roads[roadSelected].type = type;
                                           edits.roads[roadSelected].width = roadWidth;
                                           edits.rebuildRoads(*stage);
                                           modified = true;
                                         }
                                       }));
      }
      catalogPanel->add(new UISlider("Ancho", 2.0f, 16.0f, 0.5f, [&]() { return roadWidth; },
                                     [&](float v) {
                                       roadWidth = v;
                                       if (roadSelected >= 0 && roadSelected < (int)edits.roads.size() &&
                                           edits.roads[roadSelected].width != v) {
                                         edits.roads[roadSelected].width = v;
                                         edits.rebuildRoads(*stage);
                                         modified = true;
                                       }
                                     },
                                     " m"));
      catalogPanel->add(new UIButton("Carretera nueva (cortar aqui)", [&]() {
        roadSelected = pointSelected = -1;
        status = "Haz clic en el suelo para empezar otra carretera";
      }));
      catalogPanel->add(new UIButton([&]() {
        bool closed = roadSelected >= 0 && roadSelected < (int)edits.roads.size() && edits.roads[roadSelected].closed;
        return string(closed ? "Circuito cerrado: si" : "Circuito cerrado: no");
      }, [&]() {
        if (roadSelected < 0 || roadSelected >= (int)edits.roads.size())
          return;
        pushRoadUndo();
        edits.roads[roadSelected].closed = !edits.roads[roadSelected].closed;
        edits.rebuildRoads(*stage);
        modified = true;
      }));
      catalogPanel->add(new UIButton("Borrar punto elegido  (Supr)", [&]() { removeRoadPoint(); }));
      catalogPanel->add(new UIButton("Borrar carretera entera", [&]() {
        if (roadSelected < 0 || roadSelected >= (int)edits.roads.size())
          return;
        pushRoadUndo();
        edits.roads.erase(edits.roads.begin() + roadSelected);
        roadSelected = pointSelected = -1;
        edits.rebuildRoads(*stage);
        modified = true;
        status = "Carretera borrada";
      }));
      catalogPanel->add(new UILabel("Los puntos son naranjas; la carretera sigue el terreno.", UITheme::MUTED));
    } else if (tool == Tool::Prop) {
      vector<string> categories;
      for (const PropType &t : propTypes())
        if (find(categories.begin(), categories.end(), t.category) == categories.end())
          categories.push_back(t.category);
      for (const string &c : categories) {
        string category = c;
        catalogPanel->add(new UIButton([&, category]() { return (propCategory == category ? "[ " : "  ") + category + (propCategory == category ? " ]" : ""); },
                                       [&, category]() {
                                         propCategory = category;
                                         rebuildCatalog = true;
                                       }));
      }
      catalogPanel->add(new UILabel("", UITheme::MUTED));
      for (size_t i = 0; i < propTypes().size(); i++) {
        if (propTypes()[i].category != propCategory)
          continue;
        int index = (int)i;
        catalogPanel->add(new UIButton([&, index]() { return (propIndex == index ? "> " : "  ") + propTypes()[index].label; },
                                       [&, index]() {
                                         propIndex = index;
                                         placeScale = propTypes()[index].scale;
                                       }));
      }
    } else if (tool == Tool::Creature) {
      const vector<string> kinds = stage->entityKinds();
      bool any = false;
      for (size_t i = 0; i < entityTypes().size(); i++) {
        if (find(kinds.begin(), kinds.end(), entityTypes()[i].id) == kinds.end())
          continue;
        any = true;
        int index = (int)i;
        catalogPanel->add(new UIButton([&, index]() { return (entityIndex == index ? "> " : "  ") + entityTypes()[index].label; },
                                       [&, index]() { entityIndex = index; }));
      }
      if (!any)
        catalogPanel->add(new UILabel("Este mapa no admite criaturas nuevas.", UITheme::MUTED));
    } else {
      const char *names[] = {"Arena", "Asfalto", "Hierba", "Tierra"};
      for (int m = 0; m < (int)FloorMaterial::Count; m++) {
        int material = m;
        string name = names[m];
        catalogPanel->add(new UIButton([&, material, name]() { return ((int)paintMaterial == material ? "> " : "  ") + name; },
                                       [&, material]() { paintMaterial = (FloorMaterial)material; }));
      }
    }
    ui.open(catalogPanel);
    catalogPanel->moveTo(WIDTH - CATALOG_WIDTH - MARGIN, MARGIN);
  };

  {
    toolPanel = new EditorPanel("Editor de mapas", TOOL_PANEL_WIDTH);
    toolPanel->add(new UILabel([&]() { return maps[mapIndex].name + (modified ? "  *" : ""); }, UITheme::ACCENT));
    for (int t = 0; t < TOOL_COUNT; t++) {
      int index = t;
      toolPanel->add(new UIButton([&, index]() { return string((int)tool == index ? "> " : "  ") + TOOL_NAMES[index]; },
                                  [&, index]() {
                                    tool = (Tool)index;
                                    rebuildCatalog = true;
                                  }));
    }
    toolPanel->add(new UISlider("Radio del pincel", 1.0f, 80.0f, 0.5f, [&]() { return brushRadius; },
                                [&](float v) { brushRadius = v; }, " m"));
    toolPanel->add(new UISlider("Fuerza del pincel", 0.5f, 30.0f, 0.5f, [&]() { return brushStrength; },
                                [&](float v) { brushStrength = v; }, " m/s"));
    toolPanel->add(new UISlider("Tamano del objeto", 0.2f, 6.0f, 0.05f, [&]() { return selected.object ? selected.object->getScale() : placeScale; },
                                [&](float v) {
                                  placeScale = v;
                                  if (tool == Tool::Select && selected.kind == Selection::AddedProp) {
                                    selected.object->setScale(v);
                                    edits.props[selected.index].scale = v;
                                    modified = true;
                                  }
                                }));
    toolPanel->add(new UIButton([&]() { return string("Giro/tamano al azar: ") + (randomize ? "si" : "no"); },
                                [&]() { randomize = !randomize; }));
    toolPanel->add(new UISlider("Hora del dia", 0.0f, 24.0f, 0.25f, [&]() { return hour; },
                                [&](float v) {
                                  hour = v;
                                  stage->setTimeOfDay(v);
                                }));
    toolPanel->add(new UIButton("Guardar  (Ctrl+S)", [&]() { save(); }));
    toolPanel->add(new UIButton("Abrir otro mapa", [&]() { openMapList = true; }));
    toolPanel->add(new UIButton("Volver al inicio", [&]() { lookAtStart(); }));
    toolPanel->add(new UILabel([&]() { return status; }, UITheme::MUTED));
    ui.open(toolPanel);
    toolPanel->moveTo(MARGIN, MARGIN);
  }

  // --- Selecting and moving objects
  auto classify = [&](const shared_ptr<GameObject> &object) {
    Selection s;
    s.object = object;
    const auto &statics = stage->getObjects();
    const auto &dynamics = stage->getDynamicObjects();
    for (size_t i = 0; i < edits.propObjects.size(); i++)
      if (edits.propObjects[i] == object) {
        s.kind = Selection::AddedProp;
        s.index = (int)i;
        return s;
      }
    for (size_t i = 0; i < edits.entityObjects.size(); i++)
      if (edits.entityObjects[i] == object) {
        s.kind = Selection::AddedEntity;
        s.index = (int)i;
        return s;
      }
    for (size_t i = 0; i < statics.size() && i < edits.baseStatics; i++)
      if (statics[i] == object) {
        s.kind = Selection::BaseStatic;
        s.index = (int)i;
        return s;
      }
    for (size_t i = 0; i < dynamics.size() && i < edits.baseDynamics; i++)
      if (dynamics[i] == object) {
        s.kind = Selection::BaseDynamic;
        s.index = (int)i;
        return s;
      }
    return Selection();
  };
  // The object the ray hits first: solid ones, and the scenery the editor put in
  auto pick = [&](const vec3 &origin, const vec3 &dir) {
    shared_ptr<GameObject> best;
    float bestDistance = PICK_RANGE;
    auto consider = [&](const shared_ptr<GameObject> &object, bool added) {
      if (!object || !object->isVisible() || (!object->isCollidable() && !added))
        return;
      float distance;
      if (object->getShape().raycast(object->getPose(), origin, dir, distance) && distance < bestDistance) {
        best = object;
        bestDistance = distance;
      }
    };
    for (const auto &o : stage->getObjects())
      consider(o, false);
    for (const auto &o : edits.propObjects)
      consider(o, true);
    for (const auto &o : stage->getDynamicObjects())
      consider(o, true);
    if (!best)
      return Selection();
    vec3 floorPoint;
    if (terrainRay(*stage, origin, dir, floorPoint) && length(floorPoint - origin) < bestDistance)
      return Selection();
    return classify(best);
  };
  // --- Undo (Ctrl+Z). Each action pushes a function that puts things back as they were: for the
  // objects, a copy of the edit lists and the state of the objects it touched; for a terrain stroke,
  // the heights and materials it changed
  struct ObjectState {
    shared_ptr<GameObject> object;
    vec3 position;
    float heading, scale;
    bool visible, collidable;
  };
  struct ObjectSnapshot {
    vector<MapEdits::Prop> props;
    vector<MapEdits::Entity> entities;
    vector<MapEdits::Move> moves;
    vector<shared_ptr<GameObject>> propObjects, entityObjects;
  };
  auto pushUndo = [&](function<void()> action) {
    undoStack.push_back(action);
    if (undoStack.size() > 200)
      undoStack.erase(undoStack.begin());
  };
  auto snapshot = [&]() {
    ObjectSnapshot snap;
    snap.props = edits.props;
    snap.entities = edits.entities;
    snap.moves = edits.moves;
    snap.propObjects = edits.propObjects;
    snap.entityObjects = edits.entityObjects;
    return snap;
  };
  auto stateOf = [&](const shared_ptr<GameObject> &o) {
    return ObjectState{o, o->getPosition(), o->getHeading(), o->getScale(), o->isVisible(), o->isCollidable()};
  };
  // Remembers how things are, to come back to it with Ctrl+Z
  auto undoObjects = [&](const ObjectSnapshot &snap, const vector<ObjectState> &states) {
    pushUndo([&, snap, states]() {
      edits.props = snap.props;
      edits.entities = snap.entities;
      edits.moves = snap.moves;
      edits.propObjects = snap.propObjects;
      edits.entityObjects = snap.entityObjects;
      for (const ObjectState &st : states) {
        st.object->teleport(st.position);
        st.object->turn(std::remainder(st.heading - st.object->getHeading(), 2.0f * pi<float>()));
        st.object->setScale(st.scale);
        st.object->setVisible(st.visible);
        st.object->setCollidable(st.collidable);
      }
      selected = Selection();
    });
  };
  // The terrain stroke being made: what each point it touched was before
  struct Stroke {
    map<unsigned int, float> heights;
    map<unsigned int, unsigned char> materials;
    map<GameObject *, float> standing;
  };
  shared_ptr<Stroke> stroke;

  // The edit record of the selected object follows where it is now
  auto record = [&](const Selection &s) {
    if (!s.object)
      return;
    vec3 at = s.object->getPosition();
    float ground = 0.0f;
    stage->floorAt(at.x, at.z, ground);
    float yaw = s.object->getHeading();
    if (s.kind == Selection::AddedProp) {
      MapEdits::Prop &p = edits.props[s.index];
      const PropType *type = findProp(p.type);
      p.x = at.x;
      p.z = at.z;
      p.above = at.y - ground + (type ? type->sink : 0.0f);
      p.yaw = yaw;
      p.scale = s.object->getScale();
    } else if (s.kind == Selection::AddedEntity) {
      MapEdits::Entity &e = edits.entities[s.index];
      e.x = at.x;
      e.z = at.z;
      e.yaw = yaw;
    } else if (s.kind == Selection::BaseStatic || s.kind == Selection::BaseDynamic) {
      MapEdits::Move &m = edits.moveOf(s.kind == Selection::BaseDynamic, s.index);
      m.deleted = false;
      m.position = vec3(at.x, at.y - ground, at.z);
      m.yaw = yaw;
    }
    modified = true;
  };
  auto removeSelected = [&]() {
    if (!selected.object)
      return;
    undoObjects(snapshot(), {stateOf(selected.object)});
    selected.object->setVisible(false);
    selected.object->setCollidable(false);
    if (selected.kind == Selection::AddedProp) {
      edits.props.erase(edits.props.begin() + selected.index);
      edits.propObjects.erase(edits.propObjects.begin() + selected.index);
    } else if (selected.kind == Selection::AddedEntity) {
      edits.entities.erase(edits.entities.begin() + selected.index);
      edits.entityObjects.erase(edits.entityObjects.begin() + selected.index);
    } else if (selected.kind != Selection::None) {
      MapEdits::Move &m = edits.moveOf(selected.kind == Selection::BaseDynamic, selected.index);
      m.deleted = true;
    }
    modified = true;
    status = "Borrado";
    selected = Selection();
  };

  // The objects that stand on the ground near a brush stroke, with their height over it, so that
  // they rise and sink with the terrain
  map<GameObject *, float> standing;
  auto captureStanding = [&](const vec3 &centre, float radius) {
    standing.clear();
    auto consider = [&](GameObject *o, bool always) {
      vec3 p = o->getPosition();
      if (length(vec2(p.x - centre.x, p.z - centre.z)) > radius + 12.0f || !o->isVisible())
        return;
      float ground;
      if (!stage->floorAt(p.x, p.z, ground))
        return;
      if (always || (o->isCollidable() && std::fabs(p.y - ground) < STANDING_RANGE))
        standing[o] = p.y - ground;
    };
    for (const auto &o : stage->getObjects())
      consider(o.get(), false);
    for (const auto &o : edits.propObjects)
      if (o)
        consider(o.get(), true);
    for (const auto &o : edits.entityObjects)
      if (o)
        consider(o.get(), true);
  };
  auto restoreStanding = [&]() {
    for (auto &s : standing) {
      vec3 p = s.first->getPosition();
      float ground;
      if (stage->floorAt(p.x, p.z, ground))
        s.first->teleport(vec3(p.x, ground + s.second, p.z));
    }
  };

  // The end of a stroke: it can be undone
  auto finishStroke = [&]() {
    if (!stroke || (stroke->heights.empty() && stroke->materials.empty())) {
      stroke.reset();
      return;
    }
    shared_ptr<Stroke> done = stroke;
    stroke.reset();
    edits.rebuildRoads(*stage); // (the roads lie on the new ground)
    pushUndo([&, done]() {
      for (const auto &h : done->heights)
        stage->setTerrainHeight((int)(h.first % grid.nx), (int)(h.first / grid.nx), h.second);
      for (const auto &m : done->materials)
        stage->setTerrainMaterial((int)(m.first % (grid.nx - 1)), (int)(m.first / (grid.nx - 1)), (FloorMaterial)m.second);
      stage->commitTerrain();
      for (auto &o : done->standing) {
        vec3 p = o.first->getPosition();
        float ground;
        if (stage->floorAt(p.x, p.z, ground))
          o.first->teleport(vec3(p.x, ground + o.second, p.z));
      }
    });
  };

  // --- Main loop
  bool leftWas = false, rightWas = false, dragging = false, stroking = false, savedPressed = false;
  bool keyWas[GLFW_KEY_LAST + 1] = {false};
  auto pressed = [&](int key) {
    bool now = glfwGetKey(window, key) == GLFW_PRESS;
    bool edge = now && !keyWas[key];
    keyWas[key] = now;
    return edge;
  };
  float flattenTarget = 0.0f;
  double lastX = 0.0, lastY = 0.0, lastTime = glfwGetTime();
  vec3 dragOffset(0.0f);
  ObjectSnapshot dragSnapshot;
  ObjectState dragState;

  while (!glfwWindowShouldClose(window)) {
    double now = glfwGetTime();
    float dt = (float)std::min(now - lastTime, 0.1);
    lastTime = now;
    camera.resize();

    if (requestedMap >= 0) {
      int index = requestedMap;
      requestedMap = -1;
      if (loadMap(index)) {
        lookAtStart();
        rebuildCatalog = true;
      }
    }
    if (openMapList) {
      openMapList = false;
      EditorPanel *p = new EditorPanel("Abrir mapa", 300.0f);
      for (size_t m = 0; m < maps.size(); m++) {
        int index = (int)m;
        p->add(new UIButton(maps[m].name, [&, index, p]() {
          if (modified && !quitArmed) {
            status = "Hay cambios sin guardar: pulsa otra vez para descartarlos";
            quitArmed = true;
            return;
          }
          quitArmed = false;
          requestedMap = index;
          ui.close(p);
        }));
      }
      p->add(new UIButton("Cancelar", [&, p]() { ui.close(p); }));
      ui.open(p);
    }
    if (rebuildCatalog) {
      rebuildCatalog = false;
      buildCatalog();
    }

    // --- Keys
    bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    for (int t = 0; t < TOOL_COUNT; t++)
      if (pressed(GLFW_KEY_1 + t) && !ctrl) {
        tool = (Tool)t;
        rebuildCatalog = true;
      }
    if (pressed(GLFW_KEY_S) && ctrl)
      save();
    if (pressed(GLFW_KEY_Z) && ctrl) {
      if (undoStack.empty()) {
        status = "Nada que deshacer";
      } else {
        undoStack.back()();
        undoStack.pop_back();
        edits.rebuildRoads(*stage); // (terrain or roads: the roads follow the ground as it is)
        modified = true;
        status = "Deshecho";
      }
    }
    if (pressed(GLFW_KEY_DELETE) || pressed(GLFW_KEY_BACKSPACE)) {
      if (tool == Tool::Road)
        removeRoadPoint();
      else
        removeSelected();
    }
    if (pressed(GLFW_KEY_ESCAPE)) {
      if (modified && !quitArmed) {
        quitArmed = true;
        status = "Cambios sin guardar: Esc otra vez para salir sin guardar";
      } else {
        break;
      }
    }

    // --- The camera
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    bool overPanel = ui.isOverPanel((float)mx, (float)my);
    bool left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    bool leftPressed = left && !leftWas, rightPressed = right && !rightWas;
    leftWas = left;
    static bool looking = false;
    if (rightPressed && !overPanel) {
      looking = true;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      glfwGetCursorPos(window, &lastX, &lastY);
    }
    if (!right && looking) {
      looking = false;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    rightWas = right;
    if (looking) {
      yaw += (float)(mx - lastX) * 0.0035f;
      pitch = clamp(pitch + (float)(my - lastY) * 0.0035f, -1.5f, 1.5f);
      lastX = mx;
      lastY = my;
    }
    float wheel = scrollSum;
    scrollSum = 0.0f;
    if (looking && wheel != 0.0f)
      flySpeed = clamp(flySpeed * std::pow(1.2f, wheel), 2.0f, 600.0f);
    vec3 forward(std::sin(yaw) * std::cos(pitch), -std::sin(pitch), -std::cos(yaw) * std::cos(pitch));
    vec3 flat = normalize(vec3(forward.x, 0.0f, forward.z));
    vec3 rightVec(-flat.z, 0.0f, flat.x);
    {
      vec3 move(0.0f);
      if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        move += flat;
      if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && !ctrl)
        move -= flat;
      if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        move += rightVec;
      if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        move -= rightVec;
      if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        move.y += 1.0f;
      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
        move.y -= 1.0f;
      if (length(move) > 0.0f)
        eye += normalize(move) * flySpeed * (shift ? 4.0f : 1.0f) * dt;
    }
    camera.setAngles(yaw, pitch);
    camera.reposition(eye.x, eye.y, eye.z);
    camera.update();

    // --- The mouse in the world
    vec3 origin, dir, hit;
    cursorRay(window, camera, mx, my, origin, dir);
    bool hasHit = !overPanel && !looking && terrainRay(*stage, origin, dir, hit);
    bool free = !overPanel && !looking;
    bool terrainTool = tool == Tool::Raise || tool == Tool::Lower || tool == Tool::Smooth || tool == Tool::Flatten ||
                       tool == Tool::Paint;

    // The wheel: size of the brush, or turn (Shift: size) what is selected or about to be placed
    if (free && wheel != 0.0f) {
      if (terrainTool) {
        brushRadius = clamp(brushRadius * std::pow(1.1f, wheel), 1.0f, 80.0f);
      } else if (shift) {
        placeScale = clamp(placeScale * std::pow(1.08f, wheel), 0.2f, 6.0f);
        if (tool == Tool::Select && selected.kind == Selection::AddedProp) { // (the map's own keep their size)
          undoObjects(snapshot(), {stateOf(selected.object)});
          selected.object->setScale(placeScale);
          record(selected);
        }
      } else {
        placeYaw += wheel * radians(15.0f);
        if (tool == Tool::Select && selected.object) {
          undoObjects(snapshot(), {stateOf(selected.object)});
          selected.object->turn(wheel * radians(15.0f));
          record(selected);
        }
      }
    }

    hovered = Selection();
    if (tool == Tool::Select && free && !dragging)
      hovered = pick(origin, dir);

    if (tool == Tool::Select) {
      if (leftPressed && free) {
        selected = hovered;
        if (selected.object && hasHit) {
          dragging = true;
          dragSnapshot = snapshot();
          dragState = stateOf(selected.object);
          dragOffset = selected.object->getPosition() - hit;
          dragOffset.y = 0.0f;
        }
      }
      if (dragging) {
        if (!left) {
          dragging = false;
          if (length(selected.object->getPosition() - dragState.position) > 1e-4f)
            undoObjects(dragSnapshot, {dragState});
          record(selected);
        } else if (hasHit && selected.object) {
          vec3 p = selected.object->getPosition();
          float oldGround = 0.0f, newGround = 0.0f;
          stage->floorAt(p.x, p.z, oldGround);
          float x = hit.x + dragOffset.x, z = hit.z + dragOffset.z;
          if (stage->floorAt(x, z, newGround))
            selected.object->teleport(vec3(x, newGround + (p.y - oldGround), z));
        }
      }
    } else {
      dragging = false;
    }

    if (tool == Tool::Prop || tool == Tool::Creature) {
      if (leftPressed && hasHit) {
        ObjectSnapshot before = snapshot();
        size_t propsBefore = edits.props.size(), entitiesBefore = edits.entities.size();
        float yawNow = placeYaw, scaleNow = placeScale;
        if (randomize) {
          yawNow = (float)rand() / RAND_MAX * 6.2831853f;
          scaleNow = placeScale * (0.85f + 0.35f * (float)rand() / RAND_MAX);
        }
        if (tool == Tool::Prop) {
          const PropType &type = propTypes()[propIndex];
          shared_ptr<GameObject> object = makeProp(*stage, type, hit.x, hit.z, 0.0f, yawNow, scaleNow);
          if (object) {
            edits.props.push_back({type.id, hit.x, hit.z, 0.0f, yawNow, scaleNow});
            edits.propObjects.push_back(object);
            modified = true;
            status = "Puesto: " + type.label;
          } else {
            status = "No puedo cargar " + type.model;
          }
        } else {
          const vector<string> kinds = stage->entityKinds();
          const EntityType &type = entityTypes()[entityIndex];
          if (find(kinds.begin(), kinds.end(), type.id) == kinds.end()) {
            status = "Este mapa no admite: " + type.label;
          } else {
            size_t before = stage->getDynamicObjects().size();
            if (stage->spawnEntity(type.id, hit, yawNow, entityContext) && stage->getDynamicObjects().size() > before) {
              edits.entities.push_back({type.id, hit.x, hit.z, yawNow});
              edits.entityObjects.push_back(stage->getDynamicObjects().back());
              modified = true;
              status = "Puesta: " + type.label;
            } else {
              status = "No he podido crear: " + type.label;
            }
          }
        }
        // (what was put, if anything, goes away with Ctrl+Z)
        if (edits.props.size() > propsBefore)
          undoObjects(before, {ObjectState{edits.propObjects.back(), vec3(0.0f), 0.0f, 1.0f, false, false}});
        else if (edits.entities.size() > entitiesBefore)
          undoObjects(before, {ObjectState{edits.entityObjects.back(), vec3(0.0f), 0.0f, 1.0f, false, false}});
      }
    }

    // Roads: points of their splines on the screen, which one the cursor is on, and what a click does
    auto toScreen = [&](const vec3 &p, vec2 &pixel) {
      vec4 c = camera.getViewProjection() * vec4(p, 1.0f);
      if (c.w <= 0.01f)
        return false;
      int w, h;
      glfwGetWindowSize(window, &w, &h);
      pixel = vec2((c.x / c.w * 0.5f + 0.5f) * w, (0.5f - c.y / c.w * 0.5f) * h);
      return true;
    };
    auto pointAt = [&](const vec2 &p) {
      float h = 0.0f;
      stage->floorAt(p.x, p.y, h);
      return vec3(p.x, h, p.y);
    };
    pointHovered = roadHovered = -1;
    if (tool == Tool::Road && free) {
      float best = POINT_PICK;
      for (size_t r = 0; r < edits.roads.size(); r++)
        for (size_t i = 0; i < edits.roads[r].points.size(); i++) {
          vec2 pixel;
          if (!toScreen(pointAt(edits.roads[r].points[i]) + vec3(0.0f, 0.5f, 0.0f), pixel))
            continue;
          float d = length(pixel - vec2((float)mx, (float)my));
          if (d < best) {
            best = d;
            roadHovered = (int)r;
            pointHovered = (int)i;
          }
        }
    }
    if (tool == Tool::Road) {
      if (leftPressed && free) {
        if (pointHovered >= 0) {
          pushRoadUndo();
          roadSelected = roadHovered;
          pointSelected = pointHovered;
          pointDragging = true;
          roadType = edits.roads[roadSelected].type;
          roadWidth = edits.roads[roadSelected].width;
        } else if (hasHit && shift && roadSelected >= 0 && roadSelected < (int)edits.roads.size()) {
          // A point on the curve of the selected road, between the two control points it is between
          Road &road = edits.roads[roadSelected];
          vector<RoadSample> samples = sampleRoad(road, 1.0f);
          size_t nearest = 0;
          float nearestDistance = 1e30f;
          for (size_t k = 0; k < samples.size(); k++) {
            float d = length(samples[k].position - vec2(hit.x, hit.z));
            if (d < nearestDistance) {
              nearestDistance = d;
              nearest = k;
            }
          }
          if (samples.size() >= 2 && nearestDistance < std::max(road.width, 4.0f)) {
            // (a control point is on the curve: the sample nearest to it says how far along it is)
            int insertAt = 0;
            for (size_t i = 0; i < road.points.size(); i++) {
              size_t at = 0;
              float d = 1e30f;
              for (size_t k = 0; k < samples.size(); k++) {
                float e = length(samples[k].position - road.points[i]);
                if (e < d) {
                  d = e;
                  at = k;
                }
              }
              if (at <= nearest)
                insertAt = (int)i + 1;
            }
            pushRoadUndo();
            road.points.insert(road.points.begin() + insertAt, samples[nearest].position);
            pointSelected = insertAt;
            pointDragging = true;
            edits.rebuildRoads(*stage);
            modified = true;
          }
        } else if (hasHit) {
          pushRoadUndo();
          vec2 at(hit.x, hit.z);
          if (roadSelected < 0 || roadSelected >= (int)edits.roads.size() || edits.roads[roadSelected].closed) {
            Road road;
            road.type = roadType;
            road.width = roadWidth;
            edits.roads.push_back(road);
            roadSelected = (int)edits.roads.size() - 1;
          }
          Road &road = edits.roads[roadSelected];
          road.points.push_back(at);
          pointSelected = (int)road.points.size() - 1;
          edits.rebuildRoads(*stage);
          modified = true;
          status = road.points.size() < 2 ? "Anade otro punto para ver la carretera" : "Punto anadido";
        }
      }
      if (pointDragging) {
        if (!left || roadSelected < 0 || roadSelected >= (int)edits.roads.size() ||
            pointSelected >= (int)edits.roads[roadSelected].points.size()) {
          pointDragging = false;
        } else if (hasHit) {
          vec2 &p = edits.roads[roadSelected].points[pointSelected];
          if (p.x != hit.x || p.y != hit.z) {
            p = vec2(hit.x, hit.z);
            edits.rebuildRoads(*stage);
            modified = true;
          }
        }
      }
    } else {
      pointDragging = false;
    }

    // Terrain brushes
    if (terrainTool && !stage->terrainEditable() && leftPressed && free)
      status = "Este mapa no tiene terreno editable (rejilla regular)";
    if (terrainTool && stage->terrainEditable()) {
      if (leftPressed && hasHit) {
        stroking = true;
        flattenTarget = hit.y;
        captureStanding(hit, brushRadius);
        stroke = make_shared<Stroke>();
        stroke->standing = standing;
      }
      if (!left && stroking) {
        stroking = false;
        standing.clear();
        finishStroke();
      }
      if (stroking && hasHit) {
        int ix0 = std::max((int)std::floor((hit.x - brushRadius - grid.x0) / grid.dx), 0);
        int ix1 = std::min((int)std::ceil((hit.x + brushRadius - grid.x0) / grid.dx), grid.nx - 1);
        int iz0 = std::max((int)std::floor((hit.z - brushRadius - grid.z0) / grid.dz), 0);
        int iz1 = std::min((int)std::ceil((hit.z + brushRadius - grid.z0) / grid.dz), grid.nz - 1);
        if (tool == Tool::Paint) {
          for (int cz = iz0; cz < iz1; cz++)
            for (int cx = ix0; cx < ix1; cx++) {
              float px = grid.x0 + (cx + 0.5f) * grid.dx, pz = grid.z0 + (cz + 0.5f) * grid.dz;
              if (length(vec2(px - hit.x, pz - hit.z)) <= brushRadius && stage->terrainMaterial(cx, cz) != paintMaterial) {
                stroke->materials.insert({(unsigned int)(cz * (grid.nx - 1) + cx), (unsigned char)stage->terrainMaterial(cx, cz)});
                stage->setTerrainMaterial(cx, cz, paintMaterial);
                modified = true;
              }
            }
        } else {
          vector<float> next;
          vector<pair<int, int>> points;
          for (int iz = iz0; iz <= iz1; iz++)
            for (int ix = ix0; ix <= ix1; ix++) {
              float d = length(vec2(grid.x0 + ix * grid.dx - hit.x, grid.z0 + iz * grid.dz - hit.z));
              if (d >= brushRadius)
                continue;
              float w = 0.5f * (1.0f + std::cos(pi<float>() * d / brushRadius)); // 1 in the middle, 0 at the edge
              float h = stage->terrainHeight(ix, iz), target = h;
              if (tool == Tool::Raise)
                target = h + brushStrength * dt * w;
              else if (tool == Tool::Lower)
                target = h - brushStrength * dt * w;
              else if (tool == Tool::Smooth) {
                auto at = [&](int i, int j) {
                  return stage->terrainHeight(clamp(i, 0, grid.nx - 1), clamp(j, 0, grid.nz - 1));
                };
                float average = 0.25f * (at(ix - 1, iz) + at(ix + 1, iz) + at(ix, iz - 1) + at(ix, iz + 1));
                target = h + (average - h) * std::min(1.0f, w * dt * brushStrength * 0.5f);
              } else {
                target = h + (flattenTarget - h) * std::min(1.0f, w * dt * brushStrength * 0.5f);
              }
              points.push_back({ix, iz});
              next.push_back(target);
            }
          // (all computed from the heights before the stroke's step, then written)
          for (size_t k = 0; k < points.size(); k++) {
            stroke->heights.insert({(unsigned int)(points[k].second * grid.nx + points[k].first),
                                    stage->terrainHeight(points[k].first, points[k].second)});
            stage->setTerrainHeight(points[k].first, points[k].second, next[k]);
          }
          stage->commitTerrain();
          restoreStanding();
          modified = true;
        }
      }
    } else {
      stroking = false;
    }

    // --- Drawing
    ui.setHint(string(TOOL_HELP[(int)tool]));
    ui.update();
    const Environment &env = stage->getEnvironment();
    shader.use();
    light.setColor(env.lightColor.r, env.lightColor.g, env.lightColor.b);
    light.moveTo(env.lightDir.x * 100.0f, env.lightDir.y * 100.0f, env.lightDir.z * 100.0f);
    shader.setVector3("moonDir", env.lightDir.x, env.lightDir.y, env.lightDir.z);
    shader.setVector3("fogColor", env.horizon.r, env.horizon.g, env.horizon.b);
    shader.setVector3("skyZenith", env.skyZenith.r, env.skyZenith.g, env.skyZenith.b);
    shader.setVector3("sunDir", env.sunDir.x, env.sunDir.y, env.sunDir.z);
    shader.setFloat("starAlpha", env.starAlpha);
    shader.setInt("skyDunes", env.skyDunes ? 1 : 0);
    shader.setInt("canopyOn", 0);
    shader.setFloat("forestHorizon", env.forestHorizon);
    shader.setInt("spotCount", 0);
    shader.setFloat("time", (float)now);
    glClearColor(env.horizon.x, env.horizon.y, env.horizon.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    stage->render(&shader, camera.getPosition(), now);

    // The brush, the object to be placed, the selected and the hovered ones
    auto ring = [&](const vec3 &centre, float radius, const vec4 &color) {
      vec3 previous;
      for (int i = 0; i <= 48; i++) {
        float a = 2.0f * pi<float>() * i / 48.0f;
        float x = centre.x + std::cos(a) * radius, z = centre.z + std::sin(a) * radius, h = centre.y;
        stage->floorAt(x, z, h);
        vec3 p(x, h + 0.2f, z);
        if (i > 0)
          lines.line(previous, p, color);
        previous = p;
      }
    };
    auto outline = [&](const shared_ptr<GameObject> &object, const vec4 &color) {
      vec3 mn, mx2;
      object->getShape().bounds(object->getPose(), mn, mx2);
      lines.box(mn, mx2, color);
    };
    if (hasHit) {
      if (terrainTool) {
        ring(hit, brushRadius, vec4(1.0f, 1.0f, 1.0f, 0.95f));
        ring(hit, brushRadius * 0.5f, vec4(1.0f, 1.0f, 1.0f, 0.4f));
      } else if (tool == Tool::Prop || tool == Tool::Creature) {
        ring(hit, 1.0f, vec4(0.4f, 1.0f, 0.4f, 1.0f));
        float heading = randomize ? 0.0f : placeYaw;
        lines.line(hit + vec3(0, 0.3f, 0), hit + vec3(std::sin(heading), 0.3f, std::cos(heading)) * 2.5f,
                   vec4(0.4f, 1.0f, 0.4f, 1.0f));
      }
    }
    if (tool == Tool::Road) {
      for (size_t r = 0; r < edits.roads.size(); r++) {
        const Road &road = edits.roads[r];
        bool active = (int)r == roadSelected;
        vector<RoadSample> samples = sampleRoad(road, 2.0f);
        for (size_t k = 0; k + 1 < samples.size(); k++)
          lines.line(pointAt(samples[k].position) + vec3(0.0f, 0.3f, 0.0f),
                     pointAt(samples[k + 1].position) + vec3(0.0f, 0.3f, 0.0f),
                     active ? vec4(1.0f, 0.9f, 0.3f, 1.0f) : vec4(0.7f, 0.9f, 1.0f, 0.7f));
        for (size_t i = 0; i < road.points.size(); i++) {
          vec3 p = pointAt(road.points[i]);
          bool picked = active && (int)i == pointSelected;
          bool over = (int)r == roadHovered && (int)i == pointHovered;
          float size = picked || over ? 0.7f : 0.5f;
          lines.box(p - vec3(size, 0.0f, size), p + vec3(size, 2.0f * size, size),
                    picked ? vec4(1.0f, 0.5f, 0.1f, 1.0f) : over ? vec4(1.0f, 1.0f, 1.0f, 1.0f) : vec4(1.0f, 0.65f, 0.2f, 0.85f));
        }
      }
    }
    if (hovered.object && hovered.object != selected.object)
      outline(hovered.object, HOVER_COLOR);
    if (selected.object)
      outline(selected.object, SELECT_COLOR);
    lines.draw(camera);
    ui.draw();

    glfwSwapBuffers(window);
    glfwPollEvents();
    (void)savedPressed;
  }
  stage.reset();
  glfwTerminate();
  return 0;
}
