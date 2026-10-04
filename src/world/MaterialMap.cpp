#include "MaterialMap.h"

#include <algorithm>
#include <iostream>

#include "stb_image.h"

using namespace std;

shared_ptr<MaterialMap> MaterialMap::uniform(FloorMaterial material) {
  auto map = make_shared<MaterialMap>();
  map->cells.assign(1, (unsigned char)material);
  return map;
}

shared_ptr<MaterialMap> MaterialMap::loadImage(const string &path) {
  int w, h, channels;
  unsigned char *pixels = stbi_load(path.c_str(), &w, &h, &channels, 1);
  if (!pixels) {
    cerr << "MaterialMap: could not read '" << path << "'" << endl;
    return nullptr;
  }
  auto map = make_shared<MaterialMap>();
  map->width = w;
  map->height = h;
  map->cells.assign(pixels, pixels + (size_t)w * h);
  stbi_image_free(pixels);
  for (unsigned char &c : map->cells)
    if (c >= (unsigned char)FloorMaterial::Count)
      c = (unsigned char)FloorMaterial::Sand;
  return map;
}

FloorMaterial MaterialMap::at(float u, float v) const {
  int x = std::min(std::max((int)(u * width), 0), width - 1);
  int y = std::min(std::max((int)(v * height), 0), height - 1);
  return (FloorMaterial)cells[(size_t)y * width + x];
}
