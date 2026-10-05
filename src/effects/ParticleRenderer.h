#ifndef PARTICLE_RENDERER
#define PARTICLE_RENDERER

#include <memory>
#include <vector>

#include "ParticleEmitter.h"
#include "SpotLight.h"

class Camera;
class Shader;

// Draws the particles of some emitters as discs that face the camera, with
// its own small shader (shaders/particle.vert, particle.frag): everything in
// one batch, farthest first so that the translucent discs blend properly.
// They are hidden by what is in front of them (the depth test is on) but do
// not hide anything (no depth writes). Draw it after the opaque world, and
// before the interface. They are lit by the map's light (setLighting), so at
// night they are dark like everything else. Needs a current OpenGL context; it gives back the
// program and the GL state it found.
class ParticleRenderer {
private:
  std::unique_ptr<Shader> shader;
  unsigned int VAO = 0, VBO = 0, EBO = 0;
  std::vector<float> vertices; // 4 per particle
  static const int MAX_PARTICLES = 4096;
  static const int MAX_SPOTS = 8; // as in particle.frag
  glm::vec3 sunColor = glm::vec3(1.0f), sunDir = glm::vec3(0.0f, 1.0f, 0.0f);
  std::vector<SpotLight> spots;

public:
  ParticleRenderer();
  ~ParticleRenderer();
  ParticleRenderer(const ParticleRenderer &) = delete;
  ParticleRenderer &operator=(const ParticleRenderer &) = delete;

  // How many particles can be drawn at once (the farthest are left out)
  static int capacity() { return MAX_PARTICLES; }

  // The light the next draws use: the sun's colour and direction towards it
  // (the Environment's lightColor and lightDir) and the spot lights that are on
  void setLighting(const glm::vec3 &color, const glm::vec3 &direction,
                   const std::vector<SpotLight> &spotLights);

  void draw(const std::vector<std::shared_ptr<ParticleEmitter>> &emitters,
            Camera &camera);
};

#endif
