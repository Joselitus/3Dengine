#include "UITextBlock.h"

#include <algorithm>
#include <sstream>
#include <vector>

using namespace std;

#define LINE_SPACING 6.0f

UITextBlock::UITextBlock(function<string()> source, int lines,
                         function<float()> visible, glm::vec4 color)
    : source(source), visible(visible), lines(lines), color(color) {}

float UITextBlock::preferredHeight() const {
  return lines * (UIRenderer::textHeight() + LINE_SPACING) - LINE_SPACING;
}

void UITextBlock::draw(UIRenderer &renderer, const UIState &) const {
  // Wrap the whole text first, so words don't jump to the next line as
  // they appear
  string text = UIRenderer::toAscii(source());
  vector<string> wrapped(1);
  istringstream words(text);
  string word;
  while (words >> word) {
    string candidate = wrapped.back().empty() ? word : wrapped.back() + " " + word;
    if (!wrapped.back().empty() && UIRenderer::textWidth(candidate) > rect.w)
      wrapped.push_back(word);
    else
      wrapped.back() = candidate;
  }
  if ((int)wrapped.size() > lines) {
    wrapped.resize(lines);
    wrapped.back() += "...";
  }

  // Then show only the visible characters
  size_t total = 0;
  for (const string &line : wrapped)
    total += line.size();
  float fraction = visible ? min(1.0f, max(0.0f, visible())) : 1.0f;
  size_t remaining = (size_t)(fraction * total + 0.5f);
  float y = rect.y;
  for (const string &line : wrapped) {
    if (remaining == 0)
      break;
    string shown = line.substr(0, min(remaining, line.size()));
    remaining -= shown.size();
    renderer.text(rect.x, y, shown, color);
    y += UIRenderer::textHeight() + LINE_SPACING;
  }
}
