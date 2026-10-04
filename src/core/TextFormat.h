#ifndef TEXT_FORMAT
#define TEXT_FORMAT

#include <cstdarg>
#include <cstdio>
#include <string>

#include <glm/glm.hpp>

// printf-style formatting into a std::string, and short forms of vectors, for
// text shown on screen (e.g. the debug information of an object)
inline std::string textFormat(const char *format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  return buffer;
}

// `value`, or 0 if it would be shown as "-0.00" (two decimals)
inline float tidy(float value) {
  return value > -0.005f && value < 0.005f ? 0.0f : value;
}

// "(1.00, -2.50, 3.25)"
inline std::string textOf(const glm::vec3 &v) {
  return textFormat("(%.2f, %.2f, %.2f)", tidy(v.x), tidy(v.y), tidy(v.z));
}

#endif
