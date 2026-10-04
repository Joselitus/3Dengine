#include "AudioClip.h"

#include <cstdint>
#include <cstring>

using namespace std;

static uint32_t read32(const string &b, size_t at) {
  return (uint8_t)b[at] | (uint8_t)b[at + 1] << 8 | (uint8_t)b[at + 2] << 16 |
         (uint32_t)(uint8_t)b[at + 3] << 24;
}

static uint16_t read16(const string &b, size_t at) {
  return (uint8_t)b[at] | (uint8_t)b[at + 1] << 8;
}

bool AudioClip::loadWav(const string &bytes) {
  if (bytes.size() < 12 || bytes.compare(0, 4, "RIFF") != 0 ||
      bytes.compare(8, 4, "WAVE") != 0)
    return false;

  uint16_t format = 0, bits = 0;
  bool haveFormat = false;
  // Walk the chunks: "fmt " describes the samples, "data" holds them
  for (size_t at = 12; at + 8 <= bytes.size();) {
    string id = bytes.substr(at, 4);
    // Streamed WAVs (e.g. espeak-ng --stdout) may leave the size unset
    uint32_t size = read32(bytes, at + 4);
    size_t body = at + 8;
    if (id == "fmt " && body + 16 <= bytes.size()) {
      format = read16(bytes, body);
      channels = read16(bytes, body + 2);
      sampleRate = read32(bytes, body + 4);
      bits = read16(bytes, body + 14);
      haveFormat = true;
    } else if (id == "data" && haveFormat) {
      size_t end = (size == 0 || size == 0xFFFFFFFF || body + size > bytes.size())
                       ? bytes.size()
                       : body + size;
      size_t bytesPerSample = bits / 8;
      if (!bytesPerSample || !channels)
        return false;
      samples.clear();
      samples.reserve((end - body) / bytesPerSample);
      for (size_t s = body; s + bytesPerSample <= end; s += bytesPerSample) {
        if (format == 3 && bits == 32) { // IEEE float
          float v;
          memcpy(&v, &bytes[s], 4);
          samples.push_back(v);
        } else if (format == 1 && bits == 16) {
          samples.push_back((int16_t)read16(bytes, s) / 32768.0f);
        } else if (format == 1 && bits == 8) {
          samples.push_back(((uint8_t)bytes[s] - 128) / 128.0f);
        } else if (format == 1 && bits == 32) {
          samples.push_back((int32_t)read32(bytes, s) / 2147483648.0f);
        } else {
          return false; // unsupported encoding
        }
      }
      return true;
    }
    at = body + size + (size & 1); // chunks are padded to even sizes
    if (size == 0xFFFFFFFF)
      break;
  }
  return false;
}
