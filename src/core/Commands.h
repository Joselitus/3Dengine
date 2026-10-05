#ifndef COMMANDS
#define COMMANDS

#include <functional>
#include <string>
#include <vector>

// The commands that can be typed in the console (CommandConsole, key T),
// always starting with '/' (e.g. "/reset"): each
// one has a name, a line of help and what it does. A command gets the words
// typed after its name and returns a line to show (empty: nothing to say).
// main() registers them (e.g. "reset", which starts the map again); run() finds
// and runs one from a typed line.
class Commands {
public:
  using Handler =
      std::function<std::string(const std::vector<std::string> &arguments)>;

private:
  struct Command {
    std::string name, help;
    Handler handler;
  };
  std::vector<Command> commands;

public:
  void add(const std::string &name, const std::string &help, Handler handler);
  // Runs the command of `line` (its first word, which must start with '/',
  // e.g. "/reset"; the name is given to add() without it and is matched
  // ignoring case; the rest are its arguments) and returns what it says, or
  // why it couldn't run (nothing for a line that is not a command)
  std::string run(const std::string &line) const;
  // "/reset: ...", one line per command
  std::vector<std::string> describe() const;
  // The words of a line, split by spaces
  static std::vector<std::string> split(const std::string &line);
};

#endif
