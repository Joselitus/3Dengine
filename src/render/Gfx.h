#ifndef GFX
#define GFX

// Is there a graphics context? The server has none: it simulates the world without ever drawing
// it, so the models only keep their geometry (for collisions and sizes) and never touch OpenGL.
// The server sets `headless` before it loads anything; the client leaves it false.
namespace Gfx {
extern bool headless;
}

#endif
