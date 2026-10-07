#include "ParticleEmitter.h"

#include <cmath>

using namespace glm;

ParticleEmitter::ParticleEmitter(const Settings &settings, unsigned seed)
    : settings(settings), random(seed) {}

float ParticleEmitter::uniform(float lo, float hi) {
  return lo + (hi - lo) * std::uniform_real_distribution<float>(0.0f, 1.0f)(random);
}

void ParticleEmitter::setDirection(const vec3 &d) {
  float len = length(d);
  if (len > 1e-6f)
    direction = d / len;
}

void ParticleEmitter::spawn() {
  // A random direction inside the cone: the angle from the axis is spread *
  // sqrt(u) (even over the cone's cross section), the turn round it is free
  vec3 axis = direction;
  vec3 helper = fabs(axis.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0);
  vec3 side = normalize(cross(axis, helper));
  vec3 up = cross(axis, side);
  float angle = settings.spread * sqrt(uniform(0.0f, 1.0f));
  float turn = uniform(0.0f, 6.2831853f);
  vec3 dir = axis * cos(angle) +
             (side * cos(turn) + up * sin(turn)) * sin(angle);

  Particle p;
  // born a little around the emitter, so they don't come out of one point
  p.position = position + vec3(uniform(-0.1f, 0.1f), uniform(0.0f, 0.1f),
                               uniform(-0.1f, 0.1f));
  p.velocity = baseVelocity + dir * uniform(settings.speedMin, settings.speedMax);
  p.life = uniform(settings.lifeMin, settings.lifeMax);
  particles.push_back(p);
}

void ParticleEmitter::burst(int count) {
  for (int i = 0; i < count && (int)particles.size() < settings.maxParticles; i++)
    spawn();
}

void ParticleEmitter::update(double dtd) {
  float dt = (float)dtd;
  if (dt <= 0.0f)
    return;

  // Emit what is owed (a rate of 30/s at 60 frames a second is one every
  // other frame)
  owed += rate * dt;
  while (owed >= 1.0f) {
    owed -= 1.0f;
    if ((int)particles.size() < settings.maxParticles)
      spawn();
  }

  // Move them: gravity pulls them down, the air slows them, and they go when
  // their life ends or they reach the floor
  float keep = std::exp(-settings.drag * dt);
  size_t alive = 0;
  for (size_t i = 0; i < particles.size(); i++) {
    Particle p = particles[i];
    p.velocity.y -= settings.gravity * dt;
    p.velocity *= keep;
    p.position += p.velocity * dt;
    p.age += dt;
    if (p.age >= p.life)
      continue;
    float floorHeight;
    if (ground && ground(p.position.x, p.position.z, floorHeight) &&
        p.position.y < floorHeight)
      continue;
    particles[alive++] = p;
  }
  particles.resize(alive);
}
