#include "CreditsOverlay.h"

#include <cstdio>
#include <fstream>

const float CreditsOverlay::SCROLL_SPEED = 45.0f;

namespace {
const float LINE_STEP = 30.0f, GAP = 40.0f;

std::string trim(const std::string &s) {
  size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
  return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

float lineHeight(CreditsOverlay::Kind kind) {
  return kind == CreditsOverlay::Kind::Gap ? GAP : LINE_STEP;
}
} // namespace

bool CreditsOverlay::load(const std::string &path) {
  lines.clear();
  std::ifstream file(path);
  if (!file) {
    fprintf(stderr, "Credits: cannot open %s\n", path.c_str());
    return false;
  }
  std::string raw;
  bool pendingGap = false;
  while (std::getline(file, raw)) {
    std::string line = trim(raw);
    if (line.empty()) {
      pendingGap = true;
      continue;
    }
    if (line[0] == '#')
      continue;
    if (line.compare(0, 5, "title") == 0 && line.find('=') != std::string::npos) {
      lines.push_back({Kind::Title, trim(line.substr(line.find('=') + 1))});
      pendingGap = true;
      continue;
    }
    if (line.front() == '[' && line.back() == ']') {
      if (!lines.empty())
        lines.push_back({Kind::Gap, ""});
      lines.push_back({Kind::Role, trim(line.substr(1, line.size() - 2))});
      pendingGap = false;
      continue;
    }
    if (pendingGap && !lines.empty() && lines.back().kind != Kind::Gap)
      lines.push_back({Kind::Gap, ""});
    pendingGap = false;
    lines.push_back({Kind::Name, line});
  }
  return true;
}

bool CreditsOverlay::isFinished() const {
  if (!running)
    return false;
  float total = 0.0f;
  for (const Line &l : lines)
    total += lineHeight(l.kind);
  return time * SCROLL_SPEED > height + total;
}

void CreditsOverlay::draw(UIRenderer &renderer, float width, float height) const {
  if (!running)
    return;
  this->height = height;
  float y = height - time * SCROLL_SPEED;
  for (const Line &l : lines) {
    float step = lineHeight(l.kind);
    if (y > -step && y < height && !l.text.empty()) {
      glm::vec4 color = l.kind == Kind::Name    ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
                        : l.kind == Kind::Role ? glm::vec4(0.95f, 0.8f, 0.3f, 1.0f)
                                               : glm::vec4(1.0f, 0.95f, 0.8f, 1.0f);
      renderer.text((width - UIRenderer::textWidth(l.text)) / 2.0f, y, l.text, color);
    }
    y += step;
  }
}
