#ifndef PROPERTY
#define PROPERTY

#include <cstdio>
#include <functional>
#include <string>

// One value of a GameObject that the debug inspector shows and lets you change
// (GameObject::getProperties). It doesn't store the value: it reads it with
// `get` and writes it with `set` (both bound to the object), so it always shows
// the real state. Four kinds:
// - Number: a value in [min, max] (a slider); read only if it has no `set`.
// - Toggle: on or off (get/set give 1 or 0; a button that flips it).
// - Action: something to do now (a button that runs `run`).
// - Info: text to read, refreshed every frame (`text`).
struct Property {
  enum class Kind { Number, Toggle, Action, Info };

  std::string name;
  Kind kind = Kind::Info;
  std::function<float()> get;
  std::function<void(float)> set; // empty: read only
  float min = 0.0f, max = 1.0f, step = 0.0f; // step 0 = continuous
  std::string unit;
  std::function<std::string()> text; // Info
  std::function<void()> run;         // Action

  static Property number(const std::string &name, float min, float max,
                         float step, std::function<float()> get,
                         std::function<void(float)> set,
                         const std::string &unit = "") {
    Property p;
    p.name = name;
    p.kind = Kind::Number;
    p.min = min;
    p.max = max;
    p.step = step;
    p.get = get;
    p.set = set;
    p.unit = unit;
    return p;
  }
  static Property toggle(const std::string &name, std::function<bool()> get,
                         std::function<void(bool)> set) {
    Property p;
    p.name = name;
    p.kind = Kind::Toggle;
    p.get = [get]() { return get() ? 1.0f : 0.0f; };
    if (set)
      p.set = [set](float value) { set(value > 0.5f); };
    return p;
  }
  static Property action(const std::string &name, std::function<void()> run) {
    Property p;
    p.name = name;
    p.kind = Kind::Action;
    p.run = run;
    return p;
  }
  static Property info(const std::string &name,
                       std::function<std::string()> text) {
    Property p;
    p.name = name;
    p.kind = Kind::Info;
    p.text = text;
    return p;
  }

  bool isEditable() const {
    return kind == Kind::Action || (kind != Kind::Info && (bool)set);
  }
  // Its value as text ("12.3 m/s", "si", ...); empty for an Action
  std::string valueText() const {
    switch (kind) {
    case Kind::Number: {
      char number[32];
      float value = get();
      if (value > -0.05f && value < 0.05f)
        value = 0.0f; // not "-0.0"
      snprintf(number, sizeof(number), "%.1f", value);
      return unit.empty() ? number : std::string(number) + " " + unit;
    }
    case Kind::Toggle: return get() > 0.5f ? "si" : "no";
    case Kind::Info: return text();
    case Kind::Action: break;
    }
    return "";
  }
};

#endif
