#ifndef SETTINGS
#define SETTINGS

#include <map>
#include <string>

// The player's settings, kept between sessions in a text file of
// "key = value" lines ('#' starts a comment), by default
// $XDG_CONFIG_HOME/3dengine/settings.cfg (~/.config/3dengine/settings.cfg).
//
// A plain key-value store: what each key means is up to the game, e.g.
// "camera.fov" (see test.cpp and OptionsMenu). Unknown keys are kept, so
// an older version of the game doesn't erase what a newer one saved.
class Settings {
private:
  std::string path;
  std::map<std::string, std::string> values;

public:
  explicit Settings(const std::string &path = defaultPath());

  // Reads the file. A missing file is not an error (first run: defaults);
  // returns false only if it exists and can't be read. Bad lines are
  // skipped with a warning.
  bool load();
  // Writes every value, creating the directory if needed. It writes a
  // temporary file and renames it, so a crash never leaves it half written.
  bool save() const;

  bool has(const std::string &key) const { return values.count(key) > 0; }
  // `fallback` if the key is missing or is not a number
  float getFloat(const std::string &key, float fallback) const;
  void setFloat(const std::string &key, float value);
  std::string getString(const std::string &key,
                        const std::string &fallback) const;
  void setString(const std::string &key, const std::string &value);

  const std::string &getPath() const { return path; }
  static std::string defaultPath();
};

#endif
