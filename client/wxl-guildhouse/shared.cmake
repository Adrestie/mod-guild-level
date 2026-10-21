# wxl-guildhouse build glue: Dear ImGui (for the gizmo's overlay only) and ImGuizmo compiled into the
# extension, as wxl-housing does. Put (or link) this folder at wxl-core/extensions/wxl-guildhouse.

set(WXL_EXT_SHARED_SRC
    ${WXL_EXT_SHARED_SRC}
    ${CMAKE_SOURCE_DIR}/deps/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/deps/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/deps/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/deps/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/deps/imgui/backends/imgui_impl_dx9.cpp
    ${CMAKE_SOURCE_DIR}/deps/imgui/backends/imgui_impl_win32.cpp
    ${CMAKE_CURRENT_LIST_DIR}/third_party/imguizmo/ImGuizmo.cpp
)

set_property(SOURCE ${WXL_EXT_SRC} ${WXL_EXT_SHARED_SRC} APPEND PROPERTY INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/deps/imgui
    ${CMAKE_SOURCE_DIR}/deps/imgui/backends
)
