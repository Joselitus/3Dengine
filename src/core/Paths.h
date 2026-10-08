#ifndef PATHS
#define PATHS

// Shaders and assets are loaded with paths relative to src/. The binaries are built into test/,
// next to src/: this moves the process there whatever directory it was launched from. False if
// src/ can't be found (the current directory is used then).
bool enterSourceDir();

#endif
