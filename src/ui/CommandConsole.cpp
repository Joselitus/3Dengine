#include "CommandConsole.h"

#include <GLFW/glfw3.h>

#include "UILabel.h"
#include "UITextField.h"

using namespace std;

#define OUTPUT_LINES 6 // lines of output kept on screen

CommandConsole::CommandConsole(const Commands &commands,
                               vector<string> &history, float width,
                               const string &text)
    : UIPanel("Consola", width), commands(commands), history(history) {
  for (int i = 0; i < OUTPUT_LINES; i++)
    add(new UILabel(
        [this, i]() {
          // The newest at the bottom, just above the box
          size_t empty = OUTPUT_LINES - output.size();
          return (size_t)i < empty ? string() : output[i - empty];
        },
        UITheme::MUTED));
  field = add(new UITextField());
  field->setText(text);
}

void CommandConsole::print(const string &line) {
  output.push_back(line);
  while (output.size() > OUTPUT_LINES)
    output.pop_front();
}

bool CommandConsole::onChar(unsigned int codepoint) {
  browsing = -1; // typing makes it a new line
  return field->add(codepoint);
}

bool CommandConsole::onKey(int key) {
  switch (key) {
  case GLFW_KEY_ENTER:
  case GLFW_KEY_KP_ENTER: {
    string line = field->getText();
    field->clear();
    browsing = -1;
    if (Commands::split(line).empty())
      return true;
    if (history.empty() || history.back() != line)
      history.push_back(line);
    print("> " + line);
    string answer = commands.run(line);
    if (!answer.empty())
      print(answer);
    return true;
  }
  case GLFW_KEY_BACKSPACE:
    field->backspace();
    return true;
  case GLFW_KEY_UP:
    if (history.empty())
      return true;
    browsing = browsing < 0 ? (int)history.size() - 1 : max(0, browsing - 1);
    field->setText(history[browsing]);
    return true;
  case GLFW_KEY_DOWN:
    if (browsing < 0)
      return true;
    if (++browsing >= (int)history.size()) {
      browsing = -1;
      field->clear();
    } else {
      field->setText(history[browsing]);
    }
    return true;
  }
  // Esc (closes it) and the rest; their characters arrive through onChar
  return false;
}
