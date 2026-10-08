#ifndef CONNECT_MENU
#define CONNECT_MENU

#include <functional>
#include <string>

#include "UIPanel.h"

class UITextField;

// The first thing the client shows: where is the server? Two boxes, the server's address (an IP or
// a name, and a port after a colon if it is not the usual one: "192.168.1.20:7777") and the name
// the player goes by, and a button to connect. Tab goes from one box to the other, Enter connects.
// While it connects (setStatus, setBusy) the button waits; if it fails, the reason is shown and
// the player can try again. Esc ends the game.
class ConnectMenu : public UIPanel {
private:
  UITextField *address, *name;
  UITextField *focus;
  std::string status;
  bool busy = false;
  std::function<void(const std::string &, const std::string &)> connect;
  std::function<void()> quit;

  void submit();
  void setFocus(UITextField *field);

public:
  ConnectMenu(const std::string &address, const std::string &name,
              std::function<void(const std::string &, const std::string &)> connect,
              std::function<void()> quit);

  // What is shown under the boxes (progress, or why it failed)
  void setStatus(const std::string &text) { status = text; }
  // Connecting: the button does nothing until it is over
  void setBusy(bool value) { busy = value; }
  const std::string &getAddress() const;
  const std::string &getName() const;

  bool dimsBackground() const override { return false; }
  bool onKey(int key) override;
  bool onChar(unsigned int codepoint) override;
};

#endif
