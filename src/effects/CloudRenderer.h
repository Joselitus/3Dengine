#ifndef CLOUD_RENDERER
#define CLOUD_RENDERER

#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Shader;

// A cloud is just a box in the world (axis-aligned): its density comes from a 3D Worley texture
// looked up at the place of the box each sample falls on (see CloudRenderer).
struct CloudBox {
  glm::vec3 min, max;
};

// Everything that shapes the clouds' look (the cloud debugger edits it). The `texture` part is
// baked into the 3D texture by CloudRenderer::regenerate(); the rest is read every frame.
struct CloudSettings {
  // The 3D Worley texture: three octaves (R, G, B) with this many cells per side, the seed of
  // their random points, and the side of the texture in texels
  int cells[3] = {4, 8, 16};
  int seed = 0;
  int resolution = 64;
  // How much each octave counts in the shape
  glm::vec3 weights = glm::vec3(0.55f, 0.3f, 0.15f);
  // Where the box looks into the texture: texture coordinate = local * tiling + offset +
  // drift * time (the texture repeats, so any value works)
  glm::vec3 offset = glm::vec3(0.0f);
  glm::vec3 drift = glm::vec3(0.004f, 0.0f, 0.002f); // per second
  glm::vec3 tiling = glm::vec3(4.0f, 1.0f, 4.0f); // times the texture repeats across the box, per axis
  // Noise below this is a gap between blobs; optical depth of a metre of density 1; share of the
  // box (from each face) that fades out
  float threshold = 0.58f;
  float absorption = 0.09f;
  float edgeFade = 0.3f;
};

// Draws volumetric clouds over the finished picture. Each cloud is a box (as big as the map's
// whole sky if need be: a thin slab thousands of metres wide). One pass of the whole screen for
// each box (no geometry, so the camera's far plane does not cut it), and for every pixel the shader (shaders/cloud.frag)
// takes the stretch of the view ray inside the box, cut short where the scene's depth says
// something solid is in the way, picks SAMPLES equidistant points on it, averages the density of the
// Worley texture at those points (the position inside the box is the texture coordinate) and lets
// exp(-average * thickness) of the light through: that is how much of the background stays, and the
// rest is cloud. Needs a current OpenGL context. Call draw() after the world and before particles
// and the interface; it copies the depth buffer of the screen each time and gives back the
// program and the GL state it found.
class CloudRenderer {
  std::unique_ptr<Shader> shader;
  unsigned int VAO = 0, noise = 0, depth = 0;
  int depthWidth = 0, depthHeight = 0;

public:
  // Points of the ray that are averaged
  static constexpr int SAMPLES = 4;

  // The look of the clouds; after changing `cells`, `seed`, `resolution`, call regenerate()
  CloudSettings settings;

  CloudRenderer();
  CloudRenderer(const CloudRenderer &) = delete;
  CloudRenderer &operator=(const CloudRenderer &) = delete;
  ~CloudRenderer();

  // Builds the 3D texture again from settings.cells / seed / resolution (it takes a moment)
  void regenerate();

  // `viewProjection`: the camera's (Camera::getViewProjection); `lightColor`: what lights them
  // (Environment::lightColor); `sunHeight`: the sun's elevation (Environment::sunDir.y: 1 =
  // overhead, 0 = on the horizon, negative = set), which gives the clouds their colour: white by
  // day, then orange, pink and purple as it sets, dark at night; `time`: seconds (the clouds drift: settings.drift)
  void draw(const std::vector<CloudBox> &clouds, const glm::vec3 &cameraPosition,
            const glm::mat4 &viewProjection, const glm::vec3 &lightColor, float sunHeight,
            float time);
};

#endif
