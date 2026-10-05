#ifndef STAGE
#define STAGE

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "AudioClip.h"
#include "DynamicGameObject.h"
#include "FloorMaterial.h"
#include "ParticleEmitter.h"
#include "GameObject.h"
#include "MaterialMap.h"

// How the stage finds the height of its floor. Chosen when the stage is
// created and fixed for its whole life.
enum class FloorMode {
  // The floor mesh must be a regular grid of heights (x/z). The height is
  // interpolated from the grid: O(1), but no overhangs or tunnels.
  HeightField,
  // Any triangle mesh. A ray is cast straight down against the triangles,
  // which are indexed in a 2D grid so only a few are tested per query.
  DownwardRay,
};

// Collisions: every object has a collision shape (see GameObject). Space is
// divided in a fixed grid of square cells (in x/z) and objects are only tested
// against the objects that share a cell with them. Two objects never overlap:
// a collision pushes them apart (the dynamic one only, against a static one;
// the lighter one more, between two dynamic ones), and a dynamic object's
// shape is also kept above the floor, so a long vehicle doesn't clip through
// a slope. Static objects (`add`) are put on the grid when they are added and
// are not expected to move afterwards; the dynamic ones are placed on it
// every update.
//
// Everything that is loaded and placed in the world: the static scenery
// (GameObject) and the things that move (DynamicGameObject).
// Abstract: each concrete stage loads its own content and defines the rules
// applied to its dynamic objects.
class Stage {
private:
  const FloorMode floorMode;

  // Background music (null: none). The stage only holds it; the MusicPlayer
  // plays it while this is the current map.
  std::shared_ptr<const AudioClip> music;
  bool musicLoop = true;
  float musicVolume = 1.0f;
  // Ambient sound (null: none): like the music, but it stays when the music
  // is faded out (wind...)
  std::shared_ptr<const AudioClip> ambience;
  float ambienceVolume = 1.0f;

  // The collision grid
  float gridCellSize;
  struct Body {
    GameObject *object;
    DynamicGameObject *dynamic; // null for a static object
  };
  struct Cell {
    std::vector<int> statics, dynamics; // indexes in `bodies`
  };
  std::vector<Body> bodies;
  int shapeTests = 0; // shape-against-shape tests of the last update
  std::unordered_map<long long, Cell> gridCells;
  void registerBody(GameObject *object, DynamicGameObject *dynamic);
  void placeStatic(int index); // puts a static body on the grid
  void rebuildStaticGrid();
  void staticMoved(const GameObject &object); // rebuilds the grid if static
  bool cellRange(const GameObject &object, int &x0, int &z0, int &x1,
                 int &z1) const;
  void resolveCollisions();
  void collideBodies(int a, int b, std::vector<long long> &tested);
  void collideShapeWithFloor(DynamicGameObject &object) const;

  std::shared_ptr<Model> floor_mesh; // what the objects stand on
  // world-space bounds of the floor in x/z
  float minX = 0, maxX = 0, minZ = 0, maxZ = 0;
  float edgeCullMargin = 0.0f; // see setEdgeCulling

  // FloorMode::HeightField: heights[iz * nx + ix] at (x0 + ix*dx, z0 + iz*dz)
  int nx = 0, nz = 0;
  float x0 = 0, z0 = 0, dx = 1, dz = 1;
  std::vector<float> heights;

  // FloorMode::DownwardRay: world-space triangles and a grid of cells, each
  // listing the triangles whose x/z footprint overlaps it
  std::vector<glm::vec3> triVerts; // 3 per triangle
  std::vector<unsigned char> triMaterials; // the FloorMaterial of each one
  int cellsX = 0, cellsZ = 0;
  float cellSize = 1;
  std::vector<std::vector<unsigned int>> cells;

  bool buildHeightField(const std::vector<glm::vec3> &verts);
  void buildTriangleGrid();
  bool heightFieldAt(float x, float z, float &height, glm::vec3 *normal) const;
  // `triangle`, if given, gets the index of the triangle that was hit
  bool rayAt(float x, float z, float maxY, float &height, glm::vec3 *normal,
             int *triangle = nullptr) const;

  // What the floor is made of (see setFloor and materialAt)
  std::shared_ptr<const MaterialMap> floorMaterials;

  std::map<std::string, std::shared_ptr<Model>> models; // loaded only once
  std::vector<std::shared_ptr<GameObject>> objects;
  std::vector<std::shared_ptr<DynamicGameObject>> dynamicObjects;
  std::vector<std::shared_ptr<ParticleEmitter>> emitters;

  float timeOfDay = 12.0f;    // hours, 0 <= t < 24
  float dayDuration = 0.0f;   // real seconds a whole day lasts, 0 = time stands still

protected:
  // cellSize: side of the cells of the collision grid, in world units (a few
  // times the size of the biggest object is a good value)
  explicit Stage(FloorMode mode, float cellSize = 8.0f)
      : floorMode(mode), gridCellSize(cellSize) {}

  // Applied to every dynamic object each update, right after the object has
  // moved dt seconds (collisions, bounds, AI, ...)
  virtual void apply(DynamicGameObject &object, double dt) = 0;

  // Called after the time of day changed (every update while the clock runs,
  // and on setTimeOfDay). Each stage decides what the hour means: the light
  // and the sky, who is awake, a door that opens... The default does nothing.
  virtual void onTimeChanged() {}

  // Keeps a dynamic object on the floor: it can't leave the floor's bounds or
  // sink into it, and it is `grounded` while it stands on it. Meant to be
  // called from apply().
  // Objects that handle the floor themselves (DynamicGameObject::contactFloor)
  // are left to do so.
  void collideWithFloor(DynamicGameObject &object, double dt) const;

public:
  virtual ~Stage() {}

  // Loads a model, or returns it if the stage already loaded that file
  std::shared_ptr<Model> loadModel(const std::string &path);

  std::shared_ptr<GameObject> add(std::shared_ptr<GameObject> object);
  std::shared_ptr<DynamicGameObject>
  addDynamic(std::shared_ptr<DynamicGameObject> object);

  // Moves an object of the stage to `position` (GameObject::teleport). A
  // static object is put on the collision grid again where it now is (they
  // are only placed on it when added): use this, not setPosition, to move one.
  void relocate(GameObject &object, const glm::vec3 &position);
  // Turns an object of the stage `radians` around the vertical
  // (GameObject::turn); like relocate, a static one is put on the grid again
  void turn(GameObject &object, float radians);

  const std::vector<std::shared_ptr<GameObject>> &getObjects() const {
    return objects;
  }
  const std::vector<std::shared_ptr<DynamicGameObject>> &
  getDynamicObjects() const {
    return dynamicObjects;
  }

  FloorMode getFloorMode() const { return floorMode; }

  // The stage's background music: played in a loop by default, at `volume`
  // (1 = as recorded). nullptr means no music.
  void setMusic(std::shared_ptr<const AudioClip> clip, bool loop = true,
                float volume = 1.0f) {
    music = clip;
    musicLoop = loop;
    musicVolume = volume;
  }
  // Loads a WAV file as the music; false (and no music) if it can't be read
  bool loadMusic(const std::string &path, bool loop = true, float volume = 1.0f);
  // The music's volume can change while it plays (a stage fades it with
  // the time of day, for instance): the main loop hands it to the MusicPlayer
  void setMusicVolume(float volume) { musicVolume = volume; }
  // An ambient sound, played in a loop alongside the music (nullptr: none)
  void setAmbience(std::shared_ptr<const AudioClip> clip, float volume = 1.0f) {
    ambience = clip;
    ambienceVolume = volume;
  }
  bool loadAmbience(const std::string &path, float volume = 1.0f);
  std::shared_ptr<const AudioClip> getAmbience() const { return ambience; }
  float getAmbienceVolume() const { return ambienceVolume; }
  std::shared_ptr<const AudioClip> getMusic() const { return music; }
  bool isMusicLooping() const { return musicLoop; }
  float getMusicVolume() const { return musicVolume; }
  // How many pairs of shapes were tested in the last update (the grid keeps
  // it far below testing every pair)
  int getShapeTests() const { return shapeTests; }

  // Sets the floor (once): the mesh placed at `position` in the world, with no
  // rotation or scale. The lookup structure for the stage's FloorMode is
  // built here. Returns false (and the stage has no floor) if it can't be:
  // with HeightField, when the mesh is not a regular grid.
  //
  // `materials` says what the floor is made of, place by place, laid over the
  // floor's x/z bounds (see MaterialMap; MaterialMap::uniform for a floor of
  // one material). **A HeightField floor needs it** (a height field only
  // knows heights): without it setFloor fails. A DownwardRay floor can do
  // without: the material of each triangle is then taken from the name of its
  // material in the mesh file (floorMaterialFromName, e.g. "road" = asphalt).
  bool setFloor(std::shared_ptr<Model> mesh, const glm::vec3 &position,
                std::shared_ptr<const MaterialMap> materials = nullptr);
  // Moves a position (and stops a velocity) back inside the floor's bounds,
  // keeping `margin` away from the edge (e.g. the radius of a big object)
  void keepInsideFloor(glm::vec3 &position, glm::vec3 &velocity,
                       float margin = 0.0f) const;
  bool hasFloor() const { return floor_mesh != nullptr; }

  // Objects whose position is closer than `margin` to the edge of the floor are not drawn
  // (the edge of the world is not shown: what is left there is cut short). 0 = off.
  // Objects that edgeCullExempt() says no to (the player) are always drawn.
  void setEdgeCulling(float margin) { edgeCullMargin = margin; }
  virtual bool edgeCullExempt(const GameObject &object) const { return false; }

  // Lifts `point` (the centre of a sphere of `radius`, e.g. the camera) out
  // of the floor if it is below it or closer than `radius`, so that it can't
  // clip through. True if it moved it. Where there is no floor it does
  // nothing. (A floor more than a few metres above the point is ignored in
  // DownwardRay mode.)
  bool keepAboveFloor(glm::vec3 &point, float radius) const;

  // Height of the floor at (x, z) and, optionally, its (upward) normal.
  // False if there is no floor there. In DownwardRay mode the ray starts at
  // maxY and finds the highest surface at or below it, so a floor above the
  // object (a ceiling, a bridge) is ignored; HeightField mode ignores maxY.
  bool floorAt(float x, float z, float &height, glm::vec3 *normal = nullptr,
               float maxY = 1e30f) const;

  // What the floor is made of at (x, z), for things that behave differently
  // on different ground (a vehicle is slower on sand). Sand where there is no
  // floor, or the floor has no such information.
  FloorMaterial materialAt(float x, float z) const;

  // Particle emitters (dust, smoke...) of the stage: it moves their particles
  // every update and removes the ones that reach the floor. Whoever owns an
  // emitter (e.g. the RV's wheels) moves it and sets its rate; the main loop
  // draws them (ParticleRenderer).
  void addEmitter(std::shared_ptr<ParticleEmitter> emitter);
  const std::vector<std::shared_ptr<ParticleEmitter>> &getEmitters() const {
    return emitters;
  }

  // Advances every object by dt seconds, applies the stage rules to the
  // dynamic ones and resolves the collisions
  // Moves the clock (see setDayDuration), then the objects and emitters
  void update(double dt);

  // The clock of the stage, common to every map. The time of day is in hours
  // (0 = midnight, 12 = noon, wraps at 24); it advances with update() so a whole
  // day takes `seconds` of real time (0 = stopped).
  float getTimeOfDay() const { return timeOfDay; }
  void setTimeOfDay(float hours);
  float getDayDuration() const { return dayDuration; }
  void setDayDuration(float seconds) { dayDuration = seconds > 0.0f ? seconds : 0.0f; }
  // time feeds the shader's procedural animations (breathing)
  void Draw(Shader *shader, double time);
};

#endif
