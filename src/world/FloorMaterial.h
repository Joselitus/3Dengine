#ifndef FLOOR_MATERIAL
#define FLOOR_MATERIAL

#include <string>

// What the floor is made of at a place, as the game logic sees it (not how it
// is drawn): a stage answers Stage::materialAt(x, z), and whatever drives on
// or walks over it can behave accordingly (a vehicle is slower on sand).
// Add a material here, and a line for it in floorMaterialName.
enum class FloorMaterial : unsigned char {
  Sand = 0, // also what is answered where nothing else is known
  Asphalt,
  Count // number of materials, not a material
};

inline const char *floorMaterialName(FloorMaterial m) {
  switch (m) {
  case FloorMaterial::Sand: return "sand";
  case FloorMaterial::Asphalt: return "asphalt";
  case FloorMaterial::Count: break;
  }
  return "?";
}

// The material a floor mesh's own material name stands for (the .mtl names in
// an OBJ): "road" and "asphalt" are asphalt, anything else is sand
inline FloorMaterial floorMaterialFromName(const std::string &name) {
  if (name == "road" || name == "asphalt")
    return FloorMaterial::Asphalt;
  return FloorMaterial::Sand;
}

#endif
