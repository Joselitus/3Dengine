#ifndef FILM_GRAIN
#define FILM_GRAIN

#include <memory>

class Shader;

// A grainy film over the whole picture (Bob is near: see GameStage::alienPresence): flickering
// grey specks, drawn over the world and under the interface with its own small shader
// (shaders/grain.vert, grain.frag), as one triangle that covers the screen. `amount` (0 = nothing
// .. 1 = as strong as it gets) is how opaque the specks are. Needs a current OpenGL context; it
// gives back the program and the GL state it found.
class FilmGrain {
  std::unique_ptr<Shader> shader;
  unsigned int VAO = 0;

public:
  // How opaque the specks are at amount 1
  static constexpr float MAX_OPACITY = 0.3f;

  FilmGrain();
  FilmGrain(const FilmGrain &) = delete;
  FilmGrain &operator=(const FilmGrain &) = delete;
  ~FilmGrain();

  // `time`: seconds (the specks change many times a second)
  void draw(float amount, float time);
};

#endif
