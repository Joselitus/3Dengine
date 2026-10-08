#include "SoundEngine.h"

#include <iostream>

#include "miniaudio.h"

using namespace std;

// Distance attenuation of spatial sounds (world units)
#define MIN_DISTANCE 1.5f // full volume closer than this
#define MAX_DISTANCE 40.0f

// ------------------------------------------------------------------ Sound
// A data source that asks an AudioGenerator for its samples
struct GeneratorSource {
  ma_data_source_base base; // (must be first)
  AudioGenerator *generator;
};

static ma_result generatorRead(ma_data_source *ds, void *out, ma_uint64 frames,
                               ma_uint64 *framesRead) {
  GeneratorSource *source = (GeneratorSource *)ds;
  source->generator->generate((float *)out, (size_t)frames);
  if (framesRead)
    *framesRead = frames;
  return MA_SUCCESS;
}

static ma_result generatorSeek(ma_data_source *, ma_uint64) { return MA_SUCCESS; }

static ma_result generatorFormat(ma_data_source *ds, ma_format *format, ma_uint32 *channels,
                                 ma_uint32 *rate, ma_channel *, size_t) {
  GeneratorSource *source = (GeneratorSource *)ds;
  *format = ma_format_f32;
  *channels = 1;
  *rate = source->generator->sampleRate();
  return MA_SUCCESS;
}

static ma_result generatorCursor(ma_data_source *, ma_uint64 *cursor) {
  *cursor = 0;
  return MA_SUCCESS;
}

static ma_result generatorLength(ma_data_source *, ma_uint64 *length) {
  *length = 0; // endless
  return MA_NOT_IMPLEMENTED;
}

static ma_data_source_vtable generatorVtable = {generatorRead, generatorSeek, generatorFormat,
                                                generatorCursor, generatorLength, nullptr, 0};

struct Sound::Playback {
  ma_audio_buffer buffer;
  GeneratorSource source;
  ma_sound sound;
  bool hasBuffer = false, hasSource = false, hasSound = false;
};

Sound::Sound() : playback(new Playback()) {}

Sound::~Sound() {
  if (playback->hasSound)
    ma_sound_uninit(&playback->sound);
  if (playback->hasBuffer)
    ma_audio_buffer_uninit(&playback->buffer);
  if (playback->hasSource)
    ma_data_source_uninit(&playback->source.base);
}

bool Sound::isPlaying() const {
  return playback->hasSound && ma_sound_is_playing(&playback->sound) &&
         !ma_sound_at_end(&playback->sound);
}

double Sound::getCursorSeconds() const {
  float seconds = 0.0f;
  if (playback->hasSound)
    ma_sound_get_cursor_in_seconds(&playback->sound, &seconds);
  return seconds;
}

double Sound::getLengthSeconds() const { return clip ? clip->duration() : 0.0; }

void Sound::setPosition(const glm::vec3 &p) {
  if (playback->hasSound)
    ma_sound_set_position(&playback->sound, p.x, p.y, p.z);
}

void Sound::setVolume(float volume) {
  if (playback->hasSound)
    ma_sound_set_volume(&playback->sound, volume);
}

void Sound::setPitch(float pitch) {
  if (playback->hasSound)
    ma_sound_set_pitch(&playback->sound, pitch);
}

void Sound::setLooping(bool looping) {
  if (playback->hasSound)
    ma_sound_set_looping(&playback->sound, looping ? MA_TRUE : MA_FALSE);
}

void Sound::setDistances(float nearest, float farthest) {
  if (playback->hasSound) {
    ma_sound_set_min_distance(&playback->sound, nearest);
    ma_sound_set_max_distance(&playback->sound, farthest);
  }
}

void Sound::stop() {
  if (playback->hasSound)
    ma_sound_stop(&playback->sound);
}

// ------------------------------------------------------------ SoundEngine
struct SoundEngine::Groups {
  ma_sound_group group[2]; // indexed by Channel
  bool ready[2] = {false, false};
};

SoundEngine::SoundEngine() : groups(new Groups()) {
  engine = new ma_engine;
  if (ma_engine_init(nullptr, engine) != MA_SUCCESS) {
    cerr << "SoundEngine: no audio device, the game will be silent" << endl;
    delete engine;
    engine = nullptr;
    return;
  }
  for (int i = 0; i < 2; i++)
    groups->ready[i] =
        ma_sound_group_init(engine, 0, nullptr, &groups->group[i]) == MA_SUCCESS;
}

SoundEngine::~SoundEngine() {
  for (int i = 0; i < 2; i++)
    if (groups->ready[i])
      ma_sound_group_uninit(&groups->group[i]);
  if (engine) {
    ma_engine_uninit(engine);
    delete engine;
  }
}

void SoundEngine::setListener(const glm::vec3 &p, const glm::vec3 &forward) {
  if (!engine)
    return;
  ma_engine_listener_set_position(engine, 0, p.x, p.y, p.z);
  ma_engine_listener_set_direction(engine, 0, forward.x, forward.y, forward.z);
  ma_engine_listener_set_world_up(engine, 0, 0.0f, 1.0f, 0.0f);
}

void SoundEngine::setMasterVolume(float volume) {
  masterVolume = volume;
  if (engine)
    ma_engine_set_volume(engine, volume);
}

void SoundEngine::setVolume(Channel channel, float volume) {
  int i = (int)channel;
  channelVolume[i] = volume;
  if (groups->ready[i])
    ma_sound_group_set_volume(&groups->group[i], volume);
}

unique_ptr<Sound> SoundEngine::play(shared_ptr<const AudioClip> clip,
                                    bool spatial, const glm::vec3 &position,
                                    bool loop, Channel channel) {
  if (!engine || !clip || clip->frames() == 0)
    return nullptr;

  unique_ptr<Sound> s(new Sound());
  s->clip = clip;
  Sound::Playback &p = *s->playback;
  // The buffer reads the clip's samples in place (the Sound keeps the clip)
  ma_audio_buffer_config config = ma_audio_buffer_config_init(
      ma_format_f32, clip->channels, clip->frames(), clip->samples.data(),
      nullptr);
  config.sampleRate = clip->sampleRate; // the engine resamples it
  if (ma_audio_buffer_init(&config, &p.buffer) != MA_SUCCESS)
    return nullptr;
  p.hasBuffer = true;
  ma_uint32 flags = spatial ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;
  int c = (int)channel; // mixed into its channel's group (if there is one)
  ma_sound_group *group = groups->ready[c] ? &groups->group[c] : nullptr;
  if (ma_sound_init_from_data_source(engine, &p.buffer, flags, group, &p.sound) != MA_SUCCESS)
    return nullptr;
  p.hasSound = true;
  if (spatial) {
    ma_sound_set_position(&p.sound, position.x, position.y, position.z);
    ma_sound_set_min_distance(&p.sound, MIN_DISTANCE);
    ma_sound_set_max_distance(&p.sound, MAX_DISTANCE);
  }
  if (loop)
    ma_sound_set_looping(&p.sound, MA_TRUE);
  ma_sound_start(&p.sound);
  return s;
}

unique_ptr<Sound> SoundEngine::playGenerated(shared_ptr<AudioGenerator> generator,
                                             bool spatial, const glm::vec3 &position,
                                             Channel channel) {
  if (!engine || !generator)
    return nullptr;
  unique_ptr<Sound> s(new Sound());
  s->generator = generator;
  Sound::Playback &p = *s->playback;
  ma_data_source_config config = ma_data_source_config_init();
  config.vtable = &generatorVtable;
  if (ma_data_source_init(&config, &p.source.base) != MA_SUCCESS)
    return nullptr;
  p.source.generator = generator.get();
  p.hasSource = true;
  ma_uint32 flags = spatial ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;
  int c = (int)channel;
  ma_sound_group *group = groups->ready[c] ? &groups->group[c] : nullptr;
  if (ma_sound_init_from_data_source(engine, &p.source, flags, group, &p.sound) != MA_SUCCESS)
    return nullptr;
  p.hasSound = true;
  if (spatial) {
    ma_sound_set_position(&p.sound, position.x, position.y, position.z);
    ma_sound_set_min_distance(&p.sound, MIN_DISTANCE);
    ma_sound_set_max_distance(&p.sound, MAX_DISTANCE);
  }
  ma_sound_start(&p.sound);
  return s;
}
