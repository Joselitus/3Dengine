#ifndef NET_BUFFER
#define NET_BUFFER

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Binary messages of the network protocol (see Protocol.h). Numbers go out in the machine's own
// byte order (every platform the game runs on is little-endian), floats as they are.
//
// NetWriter appends values to a growing buffer; NetReader takes them back in the same order. A
// reader never reads past the end: when a message is shorter than expected (a client of another
// version...) it gives zeros and isOk() turns false, so the caller can drop the message.
class NetWriter {
private:
  std::vector<uint8_t> bytes;

  template <typename T> void put(T value) {
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&value);
    bytes.insert(bytes.end(), p, p + sizeof(T));
  }

public:
  void u8(uint8_t v) { put(v); }
  void u16(uint16_t v) { put(v); }
  void u32(uint32_t v) { put(v); }
  void i32(int32_t v) { put(v); }
  void f32(float v) { put(v); }
  void boolean(bool v) { put<uint8_t>(v ? 1 : 0); }
  void vec3(const glm::vec3 &v) {
    f32(v.x);
    f32(v.y);
    f32(v.z);
  }
  void quat(const glm::quat &q) {
    f32(q.x);
    f32(q.y);
    f32(q.z);
    f32(q.w);
  }
  void string(const std::string &s) {
    u16((uint16_t)std::min<size_t>(s.size(), 65535));
    bytes.insert(bytes.end(), s.begin(), s.begin() + std::min<size_t>(s.size(), 65535));
  }
  // Raw bytes already made (a nested message)
  void raw(const std::vector<uint8_t> &data) { bytes.insert(bytes.end(), data.begin(), data.end()); }
  // Overwrites a u16 written earlier at `offset` (to fill in a length afterwards)
  void patchU16(size_t offset, uint16_t v) { std::memcpy(&bytes[offset], &v, sizeof(v)); }

  size_t size() const { return bytes.size(); }
  const std::vector<uint8_t> &data() const { return bytes; }
  void clear() { bytes.clear(); }
};

class NetReader {
private:
  const uint8_t *p;
  const uint8_t *end;
  bool ok = true;

  template <typename T> T get() {
    T value{};
    if (!ok || (size_t)(end - p) < sizeof(T)) {
      ok = false;
      return value;
    }
    std::memcpy(&value, p, sizeof(T));
    p += sizeof(T);
    return value;
  }

public:
  NetReader(const uint8_t *data, size_t size) : p(data), end(data + size) {}
  explicit NetReader(const std::vector<uint8_t> &data) : p(data.data()), end(data.data() + data.size()) {}

  uint8_t u8() { return get<uint8_t>(); }
  uint16_t u16() { return get<uint16_t>(); }
  uint32_t u32() { return get<uint32_t>(); }
  int32_t i32() { return get<int32_t>(); }
  float f32() { return get<float>(); }
  bool boolean() { return get<uint8_t>() != 0; }
  glm::vec3 vec3() {
    float x = f32(), y = f32(), z = f32();
    return glm::vec3(x, y, z);
  }
  glm::quat quat() {
    float x = f32(), y = f32(), z = f32(), w = f32();
    return glm::quat(w, x, y, z);
  }
  std::string string() {
    uint16_t n = u16();
    if (!ok || (size_t)(end - p) < n) {
      ok = false;
      return std::string();
    }
    std::string s(reinterpret_cast<const char *>(p), n);
    p += n;
    return s;
  }
  // Skips `n` bytes
  void skip(size_t n) {
    if (!ok || (size_t)(end - p) < n) {
      ok = false;
      return;
    }
    p += n;
  }
  size_t remaining() const { return (size_t)(end - p); }
  bool isOk() const { return ok; }
};

#endif
