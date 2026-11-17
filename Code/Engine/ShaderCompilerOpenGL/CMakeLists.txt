ez_cmake_init()

ez_requires_desktop()
ez_requires_opengl()

# The OpenGL renderer executes shaders as SPIR-V that is cross-compiled to GLSL at load time.
# The SPIR-V bytecode is produced by the DXC based shader compiler toolchain, which is shared
# with the Vulkan backend (ezShaderCompilerVulkan -> ezShaderCompilerDXC). That toolchain does
# not depend on the Vulkan renderer, see EZ_BUILD_SHADERCOMPILER_DXC.
# Without the DXC toolchain there is no way to compile the HLSL shaders, so this plugin is
# only built when the toolchain is available.
ez_requires_dxc_toolchain()

# Get the name of this folder as the project name
get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

ez_create_target(LIBRARY ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
  PUBLIC
  Foundation
  RendererCore
  ShaderCompilerVulkan
)
