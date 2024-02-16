#include "imgui.h"
#include "imgui_impl_ac_window.hpp"

struct ImGui_ImplAc_Data {
  uint64_t time;
  ImVec2   virtual_cursor_pos;

  ImGui_ImplAc_Data()
  {
    memset((void*)this, 0, sizeof(*this));
  }
};

static ImGui_ImplAc_Data*
ImGui_ImplAc_GetBackendData()
{
  return ImGui::GetCurrentContext()
           ? (ImGui_ImplAc_Data*)ImGui::GetIO().BackendPlatformUserData
           : nullptr;
}

static ImGuiKey
ImGui_ImplAc_KeyToImGuiKey(int key)
{
  switch (key)
  {
  case ac_key_tab:
    return ImGuiKey_Tab;
  case ac_key_left_arrow:
    return ImGuiKey_LeftArrow;
  case ac_key_right_arrow:
    return ImGuiKey_RightArrow;
  case ac_key_up_arrow:
    return ImGuiKey_UpArrow;
  case ac_key_down_arrow:
    return ImGuiKey_DownArrow;
  case ac_key_page_up:
    return ImGuiKey_PageUp;
  case ac_key_page_down:
    return ImGuiKey_PageDown;
  case ac_key_home:
    return ImGuiKey_Home;
  case ac_key_end:
    return ImGuiKey_End;
  case ac_key_insert:
    return ImGuiKey_Insert;
  case ac_key_delete:
    return ImGuiKey_Delete;
  case ac_key_backspace:
    return ImGuiKey_Backspace;
  case ac_key_spacebar:
    return ImGuiKey_Space;
  case ac_key_return:
    return ImGuiKey_Enter;
  case ac_key_escape:
    return ImGuiKey_Escape;
  // case GLFW_KEY_APOSTROPHE: return ImGuiKey_Apostrophe;
  case ac_key_comma:
    return ImGuiKey_Comma;
  case ac_key_hyphen:
    return ImGuiKey_Minus;
  case ac_key_period:
    return ImGuiKey_Period;
  case ac_key_slash:
    return ImGuiKey_Slash;
  case ac_key_semicolon:
    return ImGuiKey_Semicolon;
  case ac_key_equal_sign:
    return ImGuiKey_Equal;
  case ac_key_open_bracket:
    return ImGuiKey_LeftBracket;
  case ac_key_backslash:
    return ImGuiKey_Backslash;
  case ac_key_close_bracket:
    return ImGuiKey_RightBracket;
  // case GLFW_KEY_GRAVE_ACCENT: return ImGuiKey_GraveAccent;
  case ac_key_caps_lock:
    return ImGuiKey_CapsLock;
  case ac_key_scroll_lock:
    return ImGuiKey_ScrollLock;
  case ac_key_keypad_num_lock:
    return ImGuiKey_NumLock;
  case ac_key_print_screen:
    return ImGuiKey_PrintScreen;
  case ac_key_pause:
    return ImGuiKey_Pause;
  case ac_key_keypad_0:
    return ImGuiKey_Keypad0;
  case ac_key_keypad_1:
    return ImGuiKey_Keypad1;
  case ac_key_keypad_2:
    return ImGuiKey_Keypad2;
  case ac_key_keypad_3:
    return ImGuiKey_Keypad3;
  case ac_key_keypad_4:
    return ImGuiKey_Keypad4;
  case ac_key_keypad_5:
    return ImGuiKey_Keypad5;
  case ac_key_keypad_6:
    return ImGuiKey_Keypad6;
  case ac_key_keypad_7:
    return ImGuiKey_Keypad7;
  case ac_key_keypad_8:
    return ImGuiKey_Keypad8;
  case ac_key_keypad_9:
    return ImGuiKey_Keypad9;
  // case : return ImGuiKey_KeypadDecimal;
  case ac_key_keypad_slash:
    return ImGuiKey_KeypadDivide;
  case ac_key_keypad_asterisk:
    return ImGuiKey_KeypadMultiply;
  case ac_key_keypad_hyphen:
    return ImGuiKey_KeypadSubtract;
  case ac_key_keypad_plus:
    return ImGuiKey_KeypadAdd;
  case ac_key_keypad_enter:
    return ImGuiKey_KeypadEnter;
  // case : return ImGuiKey_KeypadEqual;
  case ac_key_left_shift:
    return ImGuiKey_LeftShift;
  case ac_key_left_control:
    return ImGuiKey_LeftCtrl;
  case ac_key_left_alt:
    return ImGuiKey_LeftAlt;
  case ac_key_left_super:
    return ImGuiKey_LeftSuper;
  case ac_key_right_shift:
    return ImGuiKey_RightShift;
  case ac_key_right_control:
    return ImGuiKey_RightCtrl;
  case ac_key_right_alt:
    return ImGuiKey_RightAlt;
  case ac_key_right_super:
    return ImGuiKey_RightSuper;
  // case GLFW_KEY_MENU: return ImGuiKey_Menu;
  case ac_key_zero:
    return ImGuiKey_0;
  case ac_key_one:
    return ImGuiKey_1;
  case ac_key_two:
    return ImGuiKey_2;
  case ac_key_three:
    return ImGuiKey_3;
  case ac_key_four:
    return ImGuiKey_4;
  case ac_key_five:
    return ImGuiKey_5;
  case ac_key_six:
    return ImGuiKey_6;
  case ac_key_seven:
    return ImGuiKey_7;
  case ac_key_eight:
    return ImGuiKey_8;
  case ac_key_nine:
    return ImGuiKey_9;
  case ac_key_a:
    return ImGuiKey_A;
  case ac_key_b:
    return ImGuiKey_B;
  case ac_key_c:
    return ImGuiKey_C;
  case ac_key_d:
    return ImGuiKey_D;
  case ac_key_e:
    return ImGuiKey_E;
  case ac_key_f:
    return ImGuiKey_F;
  case ac_key_g:
    return ImGuiKey_G;
  case ac_key_h:
    return ImGuiKey_H;
  case ac_key_i:
    return ImGuiKey_I;
  case ac_key_j:
    return ImGuiKey_J;
  case ac_key_k:
    return ImGuiKey_K;
  case ac_key_l:
    return ImGuiKey_L;
  case ac_key_m:
    return ImGuiKey_M;
  case ac_key_n:
    return ImGuiKey_N;
  case ac_key_o:
    return ImGuiKey_O;
  case ac_key_p:
    return ImGuiKey_P;
  case ac_key_q:
    return ImGuiKey_Q;
  case ac_key_r:
    return ImGuiKey_R;
  case ac_key_s:
    return ImGuiKey_S;
  case ac_key_t:
    return ImGuiKey_T;
  case ac_key_u:
    return ImGuiKey_U;
  case ac_key_v:
    return ImGuiKey_V;
  case ac_key_w:
    return ImGuiKey_W;
  case ac_key_x:
    return ImGuiKey_X;
  case ac_key_y:
    return ImGuiKey_Y;
  case ac_key_z:
    return ImGuiKey_Z;
  case ac_key_f1:
    return ImGuiKey_F1;
  case ac_key_f2:
    return ImGuiKey_F2;
  case ac_key_f3:
    return ImGuiKey_F3;
  case ac_key_f4:
    return ImGuiKey_F4;
  case ac_key_f5:
    return ImGuiKey_F5;
  case ac_key_f6:
    return ImGuiKey_F6;
  case ac_key_f7:
    return ImGuiKey_F7;
  case ac_key_f8:
    return ImGuiKey_F8;
  case ac_key_f9:
    return ImGuiKey_F9;
  case ac_key_f10:
    return ImGuiKey_F10;
  case ac_key_f11:
    return ImGuiKey_F11;
  case ac_key_f12:
    return ImGuiKey_F12;
  default:
    return ImGuiKey_None;
  }
}

IMGUI_IMPL_API ac_result
ac_imgui_window_init(void)
{
  ImGuiIO& io = ImGui::GetIO();
  IM_ASSERT(
    io.BackendPlatformUserData == nullptr &&
    "Already initialized a platform backend!");

  ImGui_ImplAc_Data* bd = IM_NEW(ImGui_ImplAc_Data)();
  io.BackendPlatformUserData = (void*)bd;
  io.BackendPlatformName = "imgui_impl_ac";
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  ac_window_state state = ac_window_get_state();

  bd->virtual_cursor_pos.x = static_cast<float>(state.width) / 2;
  bd->virtual_cursor_pos.y = static_cast<float>(state.height) / 2;

  ImGuiStyle& style = ImGui::GetStyle();
  style = ImGuiStyle();
  style.ScaleAllSizes(ac_window_get_dpi_scale());
  io.FontGlobalScale = ac_window_get_dpi_scale();


  bd->time = 0;

  return ac_result_success;
}

IMGUI_IMPL_API void
ac_imgui_window_shutdown()
{
  ImGui_ImplAc_Data* bd = ImGui_ImplAc_GetBackendData();
  IM_ASSERT(
    bd != nullptr && "No platform backend to shutdown, or already shutdown?");
  ImGuiIO& io = ImGui::GetIO();
  io.BackendPlatformName = nullptr;
  io.BackendPlatformUserData = nullptr;
  IM_DELETE(bd);
}

IMGUI_IMPL_API void
ac_imgui_window_new_frame()
{
  ImGuiIO&           io = ImGui::GetIO();
  ImGui_ImplAc_Data* bd = ImGui_ImplAc_GetBackendData();
  IM_ASSERT(bd != nullptr && "Did you call ac_imgui_window_init()?");

  ac_window_state state = ac_window_get_state();

  io.DisplaySize = ImVec2((float)state.width, (float)state.height);

  // Setup time step
  uint64_t current_time = ac_get_time(ac_time_unit_milliseconds);
  io.DeltaTime = bd->time > 0 ? ((float)(current_time - bd->time)) / 1000.0f
                              : (float)(1.0f / 60.0f);

  if (io.DeltaTime == 0)
  {
    io.DeltaTime = (float)(1.0f / 60.0f);
  }
  bd->time = current_time;

  // Update game controllers (if enabled and available)
  // ImGui_ImplAc_UpdateGamepads();
}

IMGUI_IMPL_API void
ac_imgui_input_callback(const ac_input_event* event)
{
  ImGui_ImplAc_Data* bd = ImGui_ImplAc_GetBackendData();

  if (!bd)
  {
    return;
  }

  ImGuiIO& io = ImGui::GetIO();

  switch (event->type)
  {
  case ac_input_event_type_mouse_move:
  {
    io.MouseDrawCursor = false;

    bd->virtual_cursor_pos.x += event->mouse_move.dx;
    bd->virtual_cursor_pos.y -= event->mouse_move.dy;

    ac_window_state state = ac_window_get_state();

    bd->virtual_cursor_pos.x =
      AC_CLAMP(bd->virtual_cursor_pos.x, 0, static_cast<float>(state.width));
    bd->virtual_cursor_pos.y =
      AC_CLAMP(bd->virtual_cursor_pos.y, 0, static_cast<float>(state.height));

    float real_cursor_pos_x;
    float real_cursor_pos_y;

    if (
      ac_window_get_cursor_position(&real_cursor_pos_x, &real_cursor_pos_y) ==
      ac_result_success)
    {
      io.AddMousePosEvent(real_cursor_pos_x, real_cursor_pos_y);
    }
    else
    {
      io.MouseDrawCursor = true;
      io.AddMousePosEvent(bd->virtual_cursor_pos.x, bd->virtual_cursor_pos.y);
    }
    break;
  }
  case ac_input_event_type_mouse_button_down:
  case ac_input_event_type_mouse_button_up:
  {
    uint32_t button = -1;

    switch (event->mouse_button)
    {
    case ac_mouse_button_left:
      button = ImGuiMouseButton_Left;
      break;
    case ac_mouse_button_middle:
      button = ImGuiMouseButton_Middle;
      break;
    case ac_mouse_button_right:
      button = ImGuiMouseButton_Right;
      break;
    default:
      break;
    }
    ImGuiIO& io = ImGui::GetIO();
    if (button >= 0 && (uint32_t)button < (uint32_t)ImGuiMouseButton_COUNT)
    {
      io.AddMouseButtonEvent(
        button,
        event->type == ac_input_event_type_mouse_button_down);
    }
    break;
  }
  case ac_input_event_type_scroll:
  {
    io.AddMouseWheelEvent(event->scroll.dx, event->scroll.dy);
    break;
  }
  case ac_input_event_type_key_down:
  case ac_input_event_type_key_up:
  {
    ImGuiKey imgui_key = ImGui_ImplAc_KeyToImGuiKey(event->key);
    io.AddKeyEvent(imgui_key, event->type == ac_input_event_type_key_down);
    break;
  }
  default:
  {
    break;
  }
  }
}

IMGUI_IMPL_API void
ac_imgui_window_callback(const ac_window_event* event)
{
  ImGui_ImplAc_Data* bd = ImGui_ImplAc_GetBackendData();

  if (!bd)
  {
    return;
  }

  ImGuiIO& io = ImGui::GetIO();

  switch (event->type)
  {
  case ac_window_event_type_resize:
  {
    if (event->resize.width == 0 && event->resize.height < 0)
    {
      return;
    }

    io.DisplayFramebufferScale = ImVec2(1, 1);

    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    style.ScaleAllSizes(ac_window_get_dpi_scale());
    io.FontGlobalScale = ac_window_get_dpi_scale();

    bd->virtual_cursor_pos.x = AC_CLAMP(
      bd->virtual_cursor_pos.x,
      0,
      static_cast<float>(event->resize.width));
    bd->virtual_cursor_pos.y = AC_CLAMP(
      bd->virtual_cursor_pos.y,
      0,
      static_cast<float>(event->resize.height));
    break;
  }
  case ac_window_event_type_focus_lost:
  case ac_window_event_type_focus_own:
  {
    io.AddFocusEvent(event->type == ac_window_event_type_focus_own);
    break;
  }
  case ac_window_event_type_character:
  {
    io.AddInputCharacter(event->character);
    break;
  }
  default:
  {
    break;
  }
  }
}
