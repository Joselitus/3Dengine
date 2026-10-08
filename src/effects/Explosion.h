#ifndef EXPLOSION
#define EXPLOSION

#include <memory>
#include <vector>

#include "AudioClip.h"
#include "ParticleEmitter.h"

// What an explosion looks and sounds like, shared by everything that blows up (Mosquito, RV).
//
// makeExplosionEmitters: fire, smoke and a splash of dark drops, in that order. Each is meant to
// be sent out all at once (ParticleEmitter::burst) from where it blows up, and given to the
// stage (Stage::addEmitter).
std::vector<std::shared_ptr<ParticleEmitter>> makeExplosionEmitters(unsigned seed);
// The bang: made once (noise through a low-pass that closes, a quick attack and a long decay,
// with a low thump and some crackle)
std::shared_ptr<AudioClip> explosionBangClip();

#endif
