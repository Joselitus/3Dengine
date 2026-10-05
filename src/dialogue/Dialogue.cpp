#include "Dialogue.h"

#include "UIButton.h"
#include "UILabel.h"
#include "UIPanel.h"
#include "UITextBlock.h"

using namespace std;

Dialogue::Dialogue(const vector<string> &lines, LineNarrator &narrator,
                   const string &busyText)
    : lines(lines), narrator(narrator), busyText(busyText) {}

void Dialogue::sayCurrent() {
  if (current < lines.size())
    narrator.say(lines[current]);
}

void Dialogue::start() {
  current = 0;
  sayCurrent();
}

void Dialogue::end() { narrator.stop(); }

void Dialogue::buildPanel(UIPanel &panel) {
  // The line, revealed as the narrator delivers it
  panel.add(new UITextBlock(
      [this]() { return current < lines.size() ? lines[current] : string(); },
      TEXT_LINES, [this]() { return narrator.progress(); }));
  panel.add(new UILabel(
      [this]() {
        string where = to_string(current + 1) + "/" + to_string(lines.size());
        if (narrator.isPreparing())
          return where + "  ...";
        if (narrator.isSpeaking() && !busyText.empty())
          return where + "  " + busyText;
        return where;
      },
      UITheme::MUTED));

  // "Siguiente" until the last line, where it becomes "Cerrar"
  UIPanel *box = &panel;
  panel.add(new UIButton(
      [this]() { return isLastLine() ? string("Cerrar") : string("Siguiente"); },
      [this, box]() {
        if (isLastLine()) {
          box->requestClose(); // the InteractionSystem then calls the
          return;              // owner's onInterfaceClosed -> end()
        }
        current++;
        sayCurrent();
      }));
}
