ez_cmake_init()

# OpenGL function pointer loader. Core GL 4.5 only, no extensions.
# Functions are loaded at run-time, no link-time dependency on libGL is required.
ez_requires(EZ_BUILD_OPENGL)

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

ez_create_target(STATIC_LIBRARY ${PROJECT_NAME} NO_EZ_PREFIX)

target_include_directories(glad
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
