#include "Explosion.h"

#include <algorithm>
#include <cmath>
#include <random>

using namespace glm;
using namespace std;

namespace {
const float PI = 3.14159265f;
}

vector<shared_ptr<ParticleEmitter>> makeExplosionEmitters(unsigned seed) {
  vector<shared_ptr<ParticleEmitter>> result;

  ParticleSettings fire;
  fire.lifeMin = 0.35f;
  fire.lifeMax = 0.85f;
  fire.speedMin = 3.0f;
  fire.speedMax = 9.0f;
  fire.spread = PI; // every way
  fire.sizeStart = 0.4f;
  fire.sizeEnd = 1.8f;
  fire.color = vec3(1.0f, 0.55f, 0.12f);
  fire.alpha = 0.95f;
  fire.gravity = -3.0f; // hot: it rises
  fire.drag = 3.0f;
  fire.maxParticles = 160;
  ParticleSettings smoke;
  smoke.lifeMin = 1.5f;
  smoke.lifeMax = 3.0f;
  smoke.speedMin = 1.0f;
  smoke.speedMax = 4.0f;
  smoke.spread = PI;
  smoke.sizeStart = 0.5f;
  smoke.sizeEnd = 2.8f;
  smoke.color = vec3(0.18f, 0.16f, 0.15f);
  smoke.alpha = 0.75f;
  smoke.gravity = -1.2f;
  smoke.drag = 1.2f;
  smoke.maxParticles = 80;
  ParticleSettings splash; // fuel and blood: dark red drops that fall
  splash.lifeMin = 0.8f;
  splash.lifeMax = 1.5f;
  splash.speedMin = 4.0f;
  splash.speedMax = 10.0f;
  splash.spread = PI;
  splash.sizeStart = 0.07f;
  splash.sizeEnd = 0.05f;
  splash.color = vec3(0.30f, 0.05f, 0.03f);
  splash.alpha = 1.0f;
  splash.fadeStart = 0.85f;
  splash.gravity = 14.0f;
  splash.drag = 0.3f;
  splash.maxParticles = 220;
    result.push_back(make_shared<ParticleEmitter>(fire, seed));
  result.push_back(make_shared<ParticleEmitter>(smoke, seed + 1));
  result.push_back(make_shared<ParticleEmitter>(splash, seed + 2));
  return result;
}

// The bang of the explosion: made once (noise through a low-pass that closes, a quick attack and a
// long decay, with a low thump and some crackle), shared by every explosion
shared_ptr<AudioClip> explosionBangClip() {
  static shared_ptr<AudioClip> clip;
  if (clip)
    return clip;
  clip = make_shared<AudioClip>();
  clip->channels = 1;
  clip->sampleRate = 44100;
  const int n = 44100 * 2;
  clip->samples.resize(n);
  std::mt19937 noise(99u);
  std::uniform_real_distribution<float> white(-1.0f, 1.0f);
  float low = 0.0f, low2 = 0.0f, peak = 0.0f;
  for (int i = 0; i < n; i++) {
    float t = i / 44100.0f;
    float cutoff = 0.02f + 0.35f * std::exp(-t / 0.08f); // bright crack, then a dull roar
    low += (white(noise) - low) * cutoff;
    low2 += (low - low2) * cutoff;
    float envelope = std::min(1.0f, t / 0.003f) * std::exp(-t / 0.45f);
    float thump = 0.9f * std::sin(2.0f * PI * (48.0f - 18.0f * t) * t) * std::exp(-t / 0.35f);
    float crackle = (white(noise) > 0.995f && t < 0.6f) ? white(noise) * 0.6f * std::exp(-t / 0.3f) : 0.0f;
    float v = std::tanh(2.5f * (low2 * 3.0f * envelope + thump * std::min(1.0f, t / 0.01f) + crackle));
    clip->samples[i] = v;
    peak = std::max(peak, std::fabs(v));
  }
  for (float &v : clip->samples)
    v *= 0.9f / std::max(peak, 1e-3f);
  return clip;
}

