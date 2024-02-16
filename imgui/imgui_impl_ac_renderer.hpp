#pragma once
#include "imgui.h"
#include <ac/ac.h>

typedef struct ac_imgui_renderer_init_info {
  ac_device device;
  uint32_t  frame_count;
  uint32_t  samples;
  void (*check_ac_result_fn)(ac_result err);
} ac_imgui_renderer_init_info;

IMGUI_IMPL_API ac_result
ac_imgui_renderer_init(const ac_imgui_renderer_init_info* info);
IMGUI_IMPL_API void
ac_imgui_renderer_shutdown(void);
IMGUI_IMPL_API void
ac_imgui_renderer_new_frame(void);
IMGUI_IMPL_API void
ac_imgui_renderer_render_draw_data(
  ImDrawData* draw_data,
  ac_format   color_format,
  ac_cmd      command_buffer);

IMGUI_IMPL_API ac_result

ac_imgui_renderer_create_font_texture(void);
IMGUI_IMPL_API void
ac_imgui_renderer_destroy_font_upload_objects(void);

IMGUI_IMPL_API ImTextureID
ac_imgui_renderer_create_texture(ac_image image);

IMGUI_IMPL_API void
ac_imgui_renderer_destroy_texture(ImTextureID texture);
