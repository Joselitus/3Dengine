#ifndef PARTICLE_EMITTER
#define PARTICLE_EMITTER

#include <functional>
#include <random>
#include <vector>

#include <glm/glm.hpp>

// One speck: a disc that moves under gravity, grows and fades until its life
// is over.
struct Particle {
  glm::vec3 position;
  glm::vec3 velocity;
  float age = 0.0f;  // seconds since it was born
  float life = 1.0f; // seconds it lasts
};

// How an emitter's particles behave
struct ParticleSettings {
  float lifeMin = 0.8f, lifeMax = 1.4f;     // seconds
  float speedMin = 1.5f, speedMax = 3.5f;   // m/s, along the direction
  float spread = 0.45f;                     // radians: half angle of the cone
  float sizeStart = 0.15f, sizeEnd = 0.6f;  // radius of the disc (metres)
  glm::vec3 color = glm::vec3(0.82f, 0.70f, 0.50f);
  float alpha = 0.6f;                       // at birth; it fades to 0
  float fadeStart = 0.0f;                   // life fraction where the fade begins (0 = from birth)
  float gravity = 8.0f;                     // m/s^2 downwards
  float drag = 1.0f;                        // 1/s: the air slows them
  int maxParticles = 300;                   // alive at once
};

// A very basic particle emitter: it sends out particles at a rate, in a cone
// around a direction, from a place. It only simulates them (no OpenGL: see
// ParticleRenderer to draw them). Each particle is pulled down by gravity and
// slowed by drag, grows from sizeStart to sizeEnd (the radius of its disc) and
// fades out, and goes away when its life ends or it reaches the ground.
//
// Whoever owns the emitter moves it (setPosition, setDirection, ...) and
// switches it on and off by the rate (0 = nothing is emitted; the particles
// already out finish their life). A Stage updates the emitters it was given
// (Stage::addEmitter).
class ParticleEmitter {
public:
  typedef ParticleSettings Settings;

private:
  Settings settings;
  std::vector<Particle> particles;
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 baseVelocity = glm::vec3(0.0f);
  float rate = 0.0f;       // particles per second
  float owed = 0.0f;       // fraction of a particle not emitted yet
  std::mt19937 random;
  std::function<bool(float, float, float &)> ground;

  float uniform(float lo, float hi);
  void spawn();

public:
  explicit ParticleEmitter(const Settings &settings = Settings(),
                           unsigned seed = 1);

  // Where the particles are born
  void setPosition(const glm::vec3 &p) { position = p; }
  // The axis of the cone they are thrown along (need not be normalised)
  void setDirection(const glm::vec3 &d);
  // Added to the velocity of every new particle (e.g. the emitter's own)
  void setBaseVelocity(const glm::vec3 &v) { baseVelocity = v; }
  // Particles per second, 0 = off
  void setRate(float perSecond) { rate = perSecond > 0.0f ? perSecond : 0.0f; }
  // If set, particles that fall below the floor (the function gives its
  // height at x, z; false = no floor there) are removed
  void setGround(const std::function<bool(float, float, float &)> &g) {
    ground = g;
  }

  // Sends out `count` particles at once (an explosion), as many as fit
  void burst(int count);
  // Emits what is owed and moves the particles dt seconds
  void update(double dt);
  void clear() { particles.clear(); owed = 0.0f; }

  const std::vector<Particle> &getParticles() const { return particles; }
  const Settings &getSettings() const { return settings; }
  float getRate() const { return rate; }
  // 0 at birth .. 1 at the end of the life
  static float lifeFraction(const Particle &p) { return p.age / p.life; }
};

#endif
