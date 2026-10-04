#ifndef UI_RENDERER
#define UI_RENDERER

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Shader.h"

// Draws the 2D interface on top of the 3D scene: flat rectangles and text
// (stb_easy_font, ASCII only), in window pixels with (0, 0) at the top left.
// Everything between begin() and end() is batched and drawn in one call.
// It has its own shader (ui.vert/ui.frag) and restores the previously bound
// program, so the engine's shader stays current for its setters.
class UIRenderer {
private:
  std::unique_ptr<Shader> shader;
  unsigned int VAO, VBO;
  std::vector<float> vertices; // x, y, r, g, b, a per vertex
  glm::vec2 screen;

  void vertex(float x, float y, const glm::vec4 &color);

public:
  UIRenderer();
  UIRenderer(const UIRenderer &) = delete;
  UIRenderer &operator=(const UIRenderer &) = delete;

  // Size of the window in screen coordinates (what glfwGetCursorPos uses)
  void begin(int width, int height);
  void rect(float x, float y, float w, float h, const glm::vec4 &color);
  // Outline `thickness` pixels wide, drawn inside the rectangle
  void frame(float x, float y, float w, float h, float thickness,
             const glm::vec4 &color);
  // Text with its top left corner at (x, y)
  void text(float x, float y, const std::string &text, const glm::vec4 &color);
  void end();

  static float textWidth(const std::string &text);
  static float textHeight();
};

#endif
