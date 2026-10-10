#ifndef ROAD_MESH
#define ROAD_MESH

#include <memory>

#include "GameObject.h"
#include "Model.h"
#include "RoadSet.h"

class Stage;

// The picture of a road: a ribbon along its spline that lies on the ground (every vertex is put at
// the floor's height, a few centimetres over it) with the texture of its surface. It is a mesh of a
// fixed number of rows that can be made again when the road is edited without making new GPU
// buffers each time (the rows it does not need collapse to a point); it only grows when the road
// gets longer than it has room for.
class RoadMesh {
public:
  static constexpr int COLUMNS = 13; // vertices across the road
  static constexpr float STEP = 1.0f; // metres between rows
  static constexpr float LIFT = 0.06f; // over the floor

private:
  std::shared_ptr<Model> model = std::make_shared<Model>();
  std::shared_ptr<GameObject> object;
  size_t capacity = 0; // rows the mesh has room for
  int textureType = -1;

  void makeMesh(size_t rows, int type);

public:
  // `order` lifts later roads a little more, so that they lie over the earlier ones
  RoadMesh(Stage &stage);
  // Makes the ribbon again for `road` on the floor as it is now
  void update(const Stage &stage, const Road &road, int order);
  void setVisible(bool visible) { object->setVisible(visible); }
  const Model &getModel() const { return *model; }
};

#endif
