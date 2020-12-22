#include "imgui.h"

bool ImguiSlideFloat3_1(char const* label, float v[3], float v_min, float v_max, const char* display_format = "%.3f");
bool ImguiDragFloat3_1(char const* label, float v[3], float v_speed, const char* display_format = "%.3f");
