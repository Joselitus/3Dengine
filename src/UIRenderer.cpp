#include "UIRenderer.h"

#include <cstddef>

// Third-party, header only; it defines functions this file doesn't use
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "stb_easy_font.h"
#pragma GCC diagnostic pop

using namespace std;

// stb_easy_font glyphs are ~7 pixels tall; scaled up to be readable
#define TEXT_SCALE 2.0f
#define FLOATS_PER_VERTEX 6

// Layout of the vertices written by stb_easy_font_print
struct EasyFontVertex {
  float x, y, z;
  unsigned char color[4];
};

UIRenderer::UIRenderer() : screen(1.0f) {
  // Creating a Shader leaves it in use: give the engine's program back
  GLint previous;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previous);
  shader.reset(new Shader("ui.vert", "ui.frag"));
  glUseProgram(previous);

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                        FLOATS_PER_VERTEX * sizeof(float), (void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
                        FLOATS_PER_VERTEX * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glBindVertexArray(0);
}

void UIRenderer::begin(int width, int height) {
  screen = glm::vec2(width, height);
  vertices.clear();
}

void UIRenderer::vertex(float x, float y, const glm::vec4 &color) {
  vertices.insert(vertices.end(), {x, y, color.r, color.g, color.b, color.a});
}

void UIRenderer::rect(float x, float y, float w, float h,
                      const glm::vec4 &color) {
  vertex(x, y, color);
  vertex(x + w, y, color);
  vertex(x + w, y + h, color);
  vertex(x, y, color);
  vertex(x + w, y + h, color);
  vertex(x, y + h, color);
}

void UIRenderer::frame(float x, float y, float w, float h, float thickness,
                       const glm::vec4 &color) {
  rect(x, y, w, thickness, color);
  rect(x, y + h - thickness, w, thickness, color);
  rect(x, y + thickness, thickness, h - 2 * thickness, color);
  rect(x + w - thickness, y + thickness, thickness, h - 2 * thickness, color);
}

void UIRenderer::text(float x, float y, const string &text,
                      const glm::vec4 &color) {
  // stb_easy_font wants a mutable string and a buffer of ~270 bytes a char
  vector<char> chars(text.begin(), text.end());
  chars.push_back('\0');
  static vector<EasyFontVertex> buffer;
  buffer.resize(text.size() * 300 / sizeof(EasyFontVertex) + 4);
  unsigned char white[4] = {255, 255, 255, 255};
  int quads = stb_easy_font_print(0, 0, chars.data(), white, buffer.data(),
                                  buffer.size() * sizeof(EasyFontVertex));
  for (int q = 0; q < quads; q++) {
    const EasyFontVertex *v = &buffer[4 * q];
    const int corners[6] = {0, 1, 2, 0, 2, 3};
    for (int c : corners)
      vertex(x + v[c].x * TEXT_SCALE, y + v[c].y * TEXT_SCALE, color);
  }
}

void UIRenderer::end() {
  if (vertices.empty())
    return;

  GLint previous;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previous);
  GLboolean depth = glIsEnabled(GL_DEPTH_TEST);
  GLboolean blend = glIsEnabled(GL_BLEND);

  shader->use();
  glUniform2f(glGetUniformLocation(shader->getID(), "screen"), screen.x,
              screen.y);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
               vertices.data(), GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, vertices.size() / FLOATS_PER_VERTEX);
  glBindVertexArray(0);

  if (depth)
    glEnable(GL_DEPTH_TEST);
  if (!blend)
    glDisable(GL_BLEND);
  glUseProgram(previous);
}

float UIRenderer::textWidth(const string &text) {
  vector<char> chars(text.begin(), text.end());
  chars.push_back('\0');
  return stb_easy_font_width(chars.data()) * TEXT_SCALE;
}

float UIRenderer::textHeight() { return 8.0f * TEXT_SCALE; }
