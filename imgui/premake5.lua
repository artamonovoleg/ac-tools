include("../../ac/premake/ac_common_settings")

project("ac-imgui-shaders")
  kind("Utility")

  ac_compile_shader("imgui.acsl", "vs fs")

project("ac-imgui")
  warnings("Off")

  kind("StaticLib")

  dependson({
    "ac-imgui-shaders"
  })

  externalincludedirs({
    "../../ac/include"
  })

  files({
    "grapheditor.cpp",
    "grapheditor.h",
    "imconfig.h",
    "imcurveedit.cpp",
    "imcurveedit.h",
    "imgradient.cpp",
    "imgradient.h",
    "imgui.cpp",
    "imgui.h",
    "imgui_demo.cpp",
    "imgui_draw.cpp",
    "imgui_internal.h",
    "imgui_tables.cpp",
    "imgui_widgets.cpp",
    "imguizmo.cpp",
    "imguizmo.h",
    "imsequencer.cpp",
    "imsequencer.h",
    "imstb_rectpack.h",
    "imstb_textedit.h",
    "imstb_truetype.h",

    "imgui_impl_ac_renderer.cpp",
    "imgui_impl_ac_renderer.hpp",
    "imgui_impl_ac_window.cpp",
    "imgui_impl_ac_window.hpp",
  })
