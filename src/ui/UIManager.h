#ifndef UI_MANAGER
#define UI_MANAGER

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "myopengl.h"
#include "Interactable.h"
#include "UIOverlay.h"
#include "UIPanel.h"
#include "UIRenderer.h"

// Owns the open panels, feeds them the mouse and the keyboard and draws them
// over the scene. Also shows a one-line hint at the bottom of the screen
// (e.g. "E: use") and the overlays (UIOverlay: e.g. the debug selector).
//
// Keys: it installs the window's GLFW key and char callbacks (and user
// pointer), so nothing else may set them. Each key press goes to the top
// panel's onKey() (Backspace also when it repeats, held down), and each typed
// character to its onChar(), in the order they were typed. The character of a
// key that ran a binding is dropped (the T that opens the console must not be
// typed into it), even if it arrives in a later frame, as it does through an
// input method: until the next key is pressed. With no panel open, a typed
// character can run a binding of its own (bindChar: '/' opens the console),
// whatever key it takes on the keyboard's layout.
// if it doesn't handle it, Esc closes that panel. With no panel open, a key
// runs its binding (bindKey): the game opens the pause menu with Esc and the
// map selector with Z there.
//
// Call update() once per frame (input) and draw() after the 3D scene. While
// hasPanels(), the game should pause its own input (Controller::setEnabled).
class UIManager {
private:
  GLFWwindow *window;
  UIRenderer renderer;
  std::vector<std::unique_ptr<UIPanel>> panels; // last = on top
  std::vector<const UIOverlay *> overlays;       // not owned
  std::string hint;
  // Keys with no panel open: which key (read when a key is pressed, so it
  // can be rebound) and what it does
  struct Binding {
    std::function<int()> key;
    std::function<void()> action;
  };
  std::vector<Binding> bindings;
  struct CharBinding {
    unsigned int character;
    std::function<void()> action;
  };
  std::vector<CharBinding> charBindings;
  bool dropChar = false; // a binding ran: its key's character is not typed
  // Since the last update, from the callbacks: a key (character 0) or a
  // typed character (key -1)
  struct InputEvent {
    int key;
    unsigned int character;
  };
  std::vector<InputEvent> input;

  UIState state;              // what elements see when drawn
  UIElement *active = nullptr; // receives drag/release until the button is up
  bool buttonWasDown = false;

  static void keyCallback(GLFWwindow *window, int key, int scancode,
                          int action, int mods);
  static void charCallback(GLFWwindow *window, unsigned int codepoint);
  void placeCentered(UIPanel *panel);

public:
  explicit UIManager(GLFWwindow *window);

  // Opens a panel for `target` (built by the target itself), centred
  // vertically at the right of the screen, leaving the view in front of the
  // player (where the object usually is) clear
  UIPanel *open(Interactable &target);
  // Opens any panel (takes ownership), centred on the screen
  UIPanel *open(UIPanel *panel);
  // Closes a panel at the end of this update (safe from its own callbacks)
  void close(UIPanel *panel);
  void closeAll();
  // Whether `panel` is still open (it may have been closed by Esc or its
  // close button; the pointer is only compared, never used)
  bool isOpen(const UIPanel *panel) const;
  bool hasPanels() const { return !panels.empty(); }

  // Runs `action` when `key` (GLFW_KEY_*) is pressed and no panel is open,
  // e.g. opening a menu
  void bindKey(int key, std::function<void()> action) {
    bindKey([key]() { return key; }, action);
  }
  // Same, with a key that can change (e.g. one of the Controls, which the
  // player can rebind): `key` is asked every time a key is pressed
  void bindKey(std::function<int()> key, std::function<void()> action) {
    bindings.push_back({key, action});
  }
  // Runs `action` when `character` (a Unicode code point) is typed and no
  // panel is open: for a character that has no key of its own on every
  // layout (e.g. '/', Shift+7 on a Spanish keyboard)
  void bindChar(unsigned int character, std::function<void()> action) {
    charBindings.push_back({character, action});
  }

  void setHint(const std::string &text) { hint = text; }

  // Draws `overlay` (not owned) every frame, under the panels, until it is
  // removed; it must stay alive meanwhile
  void addOverlay(const UIOverlay *overlay) { overlays.push_back(overlay); }
  void removeOverlay(const UIOverlay *overlay);

  void update();
  void draw();
};

#endif
