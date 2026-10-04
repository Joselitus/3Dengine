#ifndef DIALOGUE
#define DIALOGUE

#include <string>
#include <vector>

#include "LineNarrator.h"

class UIPanel;

// A dialogue box: a list of lines (pages) shown one at a time, each
// delivered by a LineNarrator (a Voice for NPCs, a Typewriter for Readable
// objects) and revealed as it is delivered.
//
// Used by an Interactable: fill its panel with buildPanel(), call start()
// when the panel opens and end() when it closes. The panel has the text,
// "n/N" (plus `busyText` while the line is being delivered, e.g.
// "hablando"), and a button that says "Siguiente" and, on the last line,
// "Cerrar", which closes the panel. Esc closes it at any moment (UIManager).
// Every conversation starts again from the first line.
class Dialogue {
private:
  std::vector<std::string> lines; // UTF-8; shown in ASCII (UIRenderer)
  size_t current = 0;
  LineNarrator &narrator;
  std::string busyText;

  void sayCurrent();
  bool isLastLine() const { return current + 1 >= lines.size(); }

public:
  static const int TEXT_LINES = 4; // height of the text box, in lines

  // `narrator` must outlive the dialogue
  Dialogue(const std::vector<std::string> &lines, LineNarrator &narrator,
           const std::string &busyText = "");

  void buildPanel(UIPanel &panel);
  void start(); // from the first line
  void end();   // silences the narrator

  size_t getCurrentLine() const { return current; }
  size_t getLineCount() const { return lines.size(); }
};

#endif
