#ifndef COMMAND_CONSOLE
#define COMMAND_CONSOLE

#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "Commands.h"
#include "UIPanel.h"

class UITextField;

// The console (Action::Console, T by default; also typing '/', which it starts
// with then): a window at the bottom of the
// screen with a text box to type commands (Commands) and, above it, the last
// lines it has shown. Enter runs what is typed, Backspace deletes, the up and
// down arrows go through the commands typed before (kept while the game runs),
// and Esc closes it. Like any panel, it pauses the controls while it is open,
// so typing doesn't move the player.
class CommandConsole : public UIPanel {
private:
  const Commands &commands;
  std::vector<std::string> &history; // owned by main: it outlives the window
  int browsing = -1;                 // index in history, -1 = a new line
  std::deque<std::string> output;    // the last lines shown
  UITextField *field;
  std::function<void(const std::string &)> forward; // (a client) where the lines go besides Commands

  void print(const std::string &line);

public:
  // `text`: what the box starts with (e.g. "/", opened with the '/' key)
  // `forward`, if given, also gets every line typed (the multiplayer client sends them to the server)
  CommandConsole(const Commands &commands, std::vector<std::string> &history,
                 float width, const std::string &text = "",
                 std::function<void(const std::string &)> forward = nullptr);
  bool onKey(int key) override;
  bool onChar(unsigned int codepoint) override;
};

#endif
