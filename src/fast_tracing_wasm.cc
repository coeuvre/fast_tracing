#include <emscripten.h>

#include "imgui.h"

#define EXPORT

extern "C" {
EMSCRIPTEN_KEEPALIVE
const char *GetImGuiVersion() {
  return ImGui::GetVersion();
}
}