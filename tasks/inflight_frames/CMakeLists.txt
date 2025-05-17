add_executable(inflight_frames
  main.cpp
  App.cpp
)

target_link_libraries(inflight_frames
  PRIVATE glfw etna glm::glm wsi gui render_utils scene)

add_compile_definitions(TEXTURES_ROOT="${CMAKE_SOURCE_DIR}/resources/textures/")

target_add_shaders(inflight_frames
  shaders/quad.vert

  shaders/main.frag
  shaders/procedural.frag
)
