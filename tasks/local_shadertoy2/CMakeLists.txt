add_executable(local_shadertoy2
  main.cpp
  App.cpp
)

target_link_libraries(local_shadertoy2
  PRIVATE glfw etna glm::glm wsi gui render_utils scene)

add_compile_definitions(TEXTURES_ROOT="${CMAKE_SOURCE_DIR}/resources/textures/")

target_add_shaders(local_shadertoy2
  shaders/quad.vert

  shaders/main.frag
  shaders/procedural.frag
)
