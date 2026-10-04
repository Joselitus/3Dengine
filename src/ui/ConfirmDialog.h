#ifndef CONFIRM_DIALOG
#define CONFIRM_DIALOG

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "UIPanel.h"

// A question with a few answers, e.g. "Hay cambios sin guardar" with
// "Guardar y salir" / "Salir". Each button runs its action and closes the
// dialog. It is opened on top of the panel that asks (which stays open
// below); Esc closes just the dialog, which works as "cancel".
class ConfirmDialog : public UIPanel {
public:
  typedef std::pair<std::string, std::function<void()>> Choice;

  ConfirmDialog(const std::string &title, const std::string &message,
                const std::vector<Choice> &choices, float width = 440.0f);

  bool dimsBackground() const override { return true; }
};

#endif
