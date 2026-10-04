#ifndef MATERIAL_MAP
#define MATERIAL_MAP

#include <memory>
#include <string>
#include <vector>

#include "FloorMaterial.h"

// Which FloorMaterial the floor is made of, place by place: a grid of
// materials laid over the whole floor. A stage whose floor is a height field
// needs one (Stage::setFloor), since a height field knows nothing but heights.
//
// It is described over the unit square: (u, v) = (0, 0) is the floor's
// minimum x and z corner and (1, 1) the maximum; the Stage turns world
// positions into that. In an image, a pixel's value is its material (0 sand,
// 1 asphalt: the FloorMaterial numbers), the columns go along +x and the rows
// along +z, so the first row is the minimum z edge.
class MaterialMap {
private:
  int width = 1, height = 1;
  std::vector<unsigned char> cells; // row-major

public:
  // The whole floor is made of `material` (a floor with only one)
  static std::shared_ptr<MaterialMap> uniform(FloorMaterial material);
  // Reads an 8-bit greyscale image (PNG...). nullptr if it can't be read.
  // Values that are not a material are taken as sand.
  static std::shared_ptr<MaterialMap> loadImage(const std::string &path);

  FloorMaterial at(float u, float v) const;
  int getWidth() const { return width; }
  int getHeight() const { return height; }
};

#endif
