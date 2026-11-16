ez_cmake_init()

ez_requires_desktop()
ez_requires_opengl()

# The OpenGL renderer executes shaders as SPIR-V that is cross-compiled to GLSL at load time.
# The SPIR-V bytecode is produced by the DXC based shader compiler toolchain, which is shared
# with the Vulkan backend (ezShaderCompilerVulkan -> ezShaderCompilerDXC). That toolchain is
# only available when the Vulkan build is enabled (e.g. when libdxcompiler can be downloaded
# or provided locally), so the OpenGL shader compiler plugin is only built in that case.
# Without the DXC toolchain there is no way to compile the HLSL shaders, so the plugin is
# skipped instead of producing a broken build.
if(EZ_BUILD_VULKAN AND EZ_CMAKE_PLATFORM_SUPPORTS_VULKAN)

  # Get the name of this folder as the project name
  get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

  ez_create_target(LIBRARY ${PROJECT_NAME})

  target_link_libraries(${PROJECT_NAME}
    PUBLIC
    ShaderCompilerVulkan
  )

else()

  message(STATUS "ShaderCompilerOpenGL is skipped because the DXC based shader compiler toolchain (EZ_BUILD_VULKAN) is not available.")

endif()
