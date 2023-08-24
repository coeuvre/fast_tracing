#include <emscripten.h>

#define EXPORT EMSCRIPTEN_KEEPALIVE

EXPORT int add(int a, int b) { return a + b; }