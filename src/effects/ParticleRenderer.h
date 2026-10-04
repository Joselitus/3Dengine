#ifndef PARTICLE_RENDERER
#define PARTICLE_RENDERER

#include <memory>
#include <vector>

#include "ParticleEmitter.h"

class Camera;
class Shader;

// Draws the particles of some emitters as discs that face the camera, with
// its own small shader (shaders/particle.vert, particle.frag): everything in
// one batch, farthest first so that the translucent discs blend properly.
// They are hidden by what is in front of them (the depth test is on) but do
// not hide anything (no depth writes). Draw it after the opaque world, and
// before the interface. Needs a current OpenGL context; it gives back the
// program and the GL state it found.
class ParticleRenderer {
private:
  std::unique_ptr<Shader> shader;
  unsigned int VAO = 0, VBO = 0, EBO = 0;
  std::vector<float> vertices; // 4 per particle
  static const int MAX_PARTICLES = 4096;

public:
  ParticleRenderer();
  ~ParticleRenderer();
  ParticleRenderer(const ParticleRenderer &) = delete;
  ParticleRenderer &operator=(const ParticleRenderer &) = delete;

  // How many particles can be drawn at once (the farthest are left out)
  static int capacity() { return MAX_PARTICLES; }

  void draw(const std::vector<std::shared_ptr<ParticleEmitter>> &emitters,
            Camera &camera);
};

#endif
