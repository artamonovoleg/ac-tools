#pragma once

#include <ac/ac.h>
#include "imgui.h"

IMGUI_IMPL_API ac_result
ac_imgui_window_init(void);
IMGUI_IMPL_API void
ac_imgui_window_shutdown();
IMGUI_IMPL_API void
ac_imgui_window_new_frame();

IMGUI_IMPL_API void
ac_imgui_input_callback(const ac_input_event* event);

IMGUI_IMPL_API void
ac_imgui_window_callback(const ac_window_event* event);
