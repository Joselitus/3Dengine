#ifndef RENDER_STATS
#define RENDER_STATS

// Counters of what a frame asks of the GPU, for diagnosing slow maps (run the game with
// --profile, see main): Mesh::Draw counts draw calls and triangles, Shader::set* count uniform
// calls (each one looks the uniform up by name), GameObject::Draw counts objects. main reads
// them and clears them every frame. Plain integers: counting costs nothing worth measuring.
struct RenderStats {
  static unsigned long &draws() { static unsigned long n = 0; return n; }
  static unsigned long &triangles() { static unsigned long n = 0; return n; }
  static unsigned long &uniformCalls() { static unsigned long n = 0; return n; }
  static unsigned long &objects() { static unsigned long n = 0; return n; }
  static void clear() { draws() = triangles() = uniformCalls() = objects() = 0; }
};

#endif
