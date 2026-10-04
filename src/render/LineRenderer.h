#ifndef LINE_RENDERER
#define LINE_RENDERER

#include <memory>
#include <vector>

#include <glm/glm.hpp>

class Camera;
class Shader;

// Draws coloured line segments in the world, for debugging (the outline of a
// collision shape, a bounding box...), with its own small shader
// (shaders/lines.vert, lines.frag). Collect the segments with line(), box()...
// and draw() them all at once; the list is then emptied. They are drawn over
// everything (no depth test), so they show through walls. Needs a current
// OpenGL context; it gives back the program and the GL state it found.
class LineRenderer {
private:
  std::unique_ptr<Shader> shader;
  unsigned int VAO = 0, VBO = 0;
  size_t capacity = 0;         // vertices the buffer can hold
  std::vector<float> vertices; // x, y, z, r, g, b, a per vertex

public:
  LineRenderer();
  ~LineRenderer();
  LineRenderer(const LineRenderer &) = delete;
  LineRenderer &operator=(const LineRenderer &) = delete;

  void line(const glm::vec3 &a, const glm::vec3 &b, const glm::vec4 &color);
  // The 12 edges of an axis-aligned box
  void box(const glm::vec3 &min, const glm::vec3 &max, const glm::vec4 &color);
  // The 12 edges of an oriented box: centre, axes (unit) and half sizes
  void box(const glm::vec3 &centre, const glm::mat3 &axes,
           const glm::vec3 &half, const glm::vec4 &color);
  // A circle around `normal` (unit), in `segments` straight pieces
  void circle(const glm::vec3 &centre, const glm::vec3 &normal, float radius,
              const glm::vec4 &color, int segments = 24);
  // Half a circle in the plane of `u` and `v` (unit, perpendicular), the
  // side towards `v`
  void arc(const glm::vec3 &centre, const glm::vec3 &u, const glm::vec3 &v,
           float radius, const glm::vec4 &color, int segments = 12);

  // Draws the collected lines and forgets them
  void draw(Camera &camera);
};

#endif
