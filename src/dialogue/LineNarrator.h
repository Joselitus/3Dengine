#ifndef LINE_NARRATOR
#define LINE_NARRATOR

#include <string>

#include <glm/glm.hpp>

// How a Dialogue delivers each line (abstract). It tells how much of the
// line has been delivered so far, so the dialogue box shows the text as it
// goes:
//   Voice       says it out loud (text to speech), for NPCs
//   Typewriter  shows it letter by letter, silently, for things you read
class LineNarrator {
public:
  virtual ~LineNarrator() {}

  // Starts delivering `text` (UTF-8), replacing the current line
  virtual void say(const std::string &text) = 0;
  virtual void stop() = 0;
  // Every frame; `position`: where it comes from (for sound)
  virtual void update(double dt, const glm::vec3 &position) = 0;

  // Fraction of the line delivered: 0 before it starts, 1 when done
  virtual float progress() const = 0;
  // Still delivering the line (or getting ready to)
  virtual bool isSpeaking() const = 0;
  // Getting ready, nothing delivered yet (e.g. synthesizing the voice)
  virtual bool isPreparing() const { return false; }
};

#endif
