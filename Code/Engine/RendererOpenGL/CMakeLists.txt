ez_cmake_init()


ez_requires_desktop()
ez_requires_opengl()

# Get the name of this folder as the project name
get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

ez_create_target(LIBRARY ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
  PRIVATE
  spirv-cross-glsl
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
  Foundation
  RendererFoundation
  Core
  glad
)

if(EZ_3RDPARTY_GLFW_SUPPORT)
  target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
endif()
