#include "UILabel.h"

using namespace std;

UILabel::UILabel(const string &text, glm::vec4 color)
    : source([text]() { return text; }), color(color) {}

UILabel::UILabel(function<string()> source, glm::vec4 color)
    : source(source), color(color) {}

float UILabel::preferredHeight() const { return UIRenderer::textHeight(); }

void UILabel::draw(UIRenderer &renderer, const UIState &) const {
  renderer.text(rect.x, rect.y, source(), color);
}
