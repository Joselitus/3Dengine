#include "Commands.h"

#include <cctype>
#include <sstream>

using namespace std;

static string lower(string text) {
  for (char &c : text)
    c = (char)tolower((unsigned char)c);
  return text;
}

void Commands::add(const string &name, const string &help, Handler handler) {
  commands.push_back({lower(name), help, handler});
}

vector<string> Commands::split(const string &line) {
  vector<string> words;
  istringstream in(line);
  string word;
  while (in >> word)
    words.push_back(word);
  return words;
}

string Commands::run(const string &line) const {
  vector<string> words = split(line);
  if (words.empty())
    return "";
  // A command starts with '/'; anything else is not one (and says nothing)
  if (words[0][0] != '/')
    return "";
  string name = lower(words[0].substr(1));
  words.erase(words.begin());
  for (const Command &command : commands)
    if (command.name == name)
      return command.handler(words);
  return "Comando desconocido: /" + name;
}

vector<string> Commands::describe() const {
  vector<string> lines;
  for (const Command &command : commands)
    lines.push_back("/" + command.name + ": " + command.help);
  return lines;
}
