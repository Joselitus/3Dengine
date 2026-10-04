// miniaudio implementation (third-party, public domain; see miniaudio.h).
// The backends (PulseAudio, ALSA...) are loaded at runtime, so nothing extra
// is linked besides -ldl -lpthread -lm.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
