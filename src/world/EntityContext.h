#ifndef ENTITY_CONTEXT
#define ENTITY_CONTEXT

class SoundEngine;
class SpeechSynthesizer;

// What a stage needs to make a creature or an NPC after the map is built (GameStage::spawnEntity):
// the same things the map's own constructor was given
struct EntityContext {
  SoundEngine &sound;
  SpeechSynthesizer &speech;
};

#endif
