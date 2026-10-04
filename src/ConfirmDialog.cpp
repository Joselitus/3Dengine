#include "ConfirmDialog.h"

#include "UIButton.h"
#include "UIRow.h"
#include "UITextBlock.h"

using namespace std;

ConfirmDialog::ConfirmDialog(const string &title, const string &message,
                             const vector<Choice> &choices, float width)
    : UIPanel(title, width, false) {
  add(new UITextBlock([message]() { return message; }, 3));
  UIRow *buttons = add(new UIRow());
  for (const Choice &choice : choices) {
    function<void()> action = choice.second;
    buttons->add(new UIButton(choice.first, [this, action]() {
      if (action)
        action();
      requestClose();
    }));
  }
}
