#include "Settings.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

using namespace std;

static string trim(const string &s) {
  size_t a = s.find_first_not_of(" \t\r");
  size_t b = s.find_last_not_of(" \t\r");
  return a == string::npos ? string() : s.substr(a, b - a + 1);
}

// Creates `dir` and its parents (like mkdir -p)
static bool makeDirectories(const string &dir) {
  for (size_t i = 1; i <= dir.size(); i++)
    if (i == dir.size() || dir[i] == '/') {
      string part = dir.substr(0, i);
      if (mkdir(part.c_str(), 0755) != 0 && errno != EEXIST)
        return false;
    }
  return true;
}

string Settings::defaultPath() {
  const char *config = getenv("XDG_CONFIG_HOME");
  string base;
  if (config && config[0])
    base = config;
  else if (const char *home = getenv("HOME"))
    base = string(home) + "/.config";
  else
    base = ".";
  return base + "/3dengine/settings.cfg";
}

Settings::Settings(const string &path) : path(path) {}

bool Settings::load() {
  ifstream in(path);
  if (!in) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
      return true; // no file yet: defaults
    cerr << path << ": cannot read the settings" << endl;
    return false;
  }
  string line;
  int number = 0;
  while (getline(in, line)) {
    number++;
    size_t comment = line.find('#');
    if (comment != string::npos)
      line.erase(comment);
    if (trim(line).empty())
      continue;
    size_t equals = line.find('=');
    string key = equals == string::npos ? "" : trim(line.substr(0, equals));
    if (key.empty()) {
      cerr << path << ":" << number << ": expected 'key = value', skipped"
           << endl;
      continue;
    }
    values[key] = trim(line.substr(equals + 1));
  }
  return true;
}

bool Settings::save() const {
  size_t slash = path.find_last_of('/');
  if (slash != string::npos && !makeDirectories(path.substr(0, slash))) {
    cerr << path << ": cannot create its directory" << endl;
    return false;
  }
  string temporary = path + ".tmp";
  {
    ofstream out(temporary);
    if (!out) {
      cerr << temporary << ": cannot write the settings" << endl;
      return false;
    }
    out << "# 3Dengine settings, written by the game (see Settings.h)\n";
    for (const auto &entry : values)
      out << entry.first << " = " << entry.second << "\n";
    if (!out.good())
      return false;
  }
  if (rename(temporary.c_str(), path.c_str()) != 0) {
    cerr << path << ": cannot write the settings" << endl;
    return false;
  }
  return true;
}

float Settings::getFloat(const string &key, float fallback) const {
  auto it = values.find(key);
  if (it == values.end())
    return fallback;
  istringstream in(it->second);
  float value;
  if (!(in >> value) || !(in >> ws).eof())
    return fallback;
  return value;
}

void Settings::setFloat(const string &key, float value) {
  ostringstream out;
  out << value;
  values[key] = out.str();
}

string Settings::getString(const string &key, const string &fallback) const {
  auto it = values.find(key);
  return it == values.end() ? fallback : it->second;
}

void Settings::setString(const string &key, const string &value) {
  values[key] = value;
}
