# #####################################
# ## Vulkan support
# #####################################

# The Vulkan RENDERER only builds on Windows and Android; on Linux and OSX it does not compile
# (RendererVulkan has #error guards for those platforms). Default the option accordingly so that
# a default configure on Linux does not attempt to build it. The DXC based shader compiler
# toolchain is independent of the renderer, see EZ_BUILD_SHADERCOMPILER_DXC below.
# NOTE: this file is included before project(), so CMAKE_SYSTEM_NAME is not available yet;
# query the host OS instead (fine for native builds, which is how these options are consumed).
cmake_host_system_information(RESULT _EZ_HOST_OS QUERY OS_NAME)
if(_EZ_HOST_OS STREQUAL "Linux" OR _EZ_HOST_OS STREQUAL "Darwin")
	set(_EZ_BUILD_VULKAN_DEFAULT OFF)
else()
	set(_EZ_BUILD_VULKAN_DEFAULT ON)
endif()
unset(_EZ_HOST_OS)
set(EZ_BUILD_VULKAN ${_EZ_BUILD_VULKAN_DEFAULT} CACHE BOOL "Whether to build Vulkan renderer support.")
unset(_EZ_BUILD_VULKAN_DEFAULT)

# The DXC based shader compiler toolchain (ShaderCompilerDXC, ShaderCompilerVulkan, ShaderCompilerOpenGL)
# does NOT depend on the Vulkan renderer. On Linux and OSX the Vulkan RENDERER is not supported
# (RendererVulkan is Windows/Android only), but the shader compilers are useful there (e.g. to
# cross-compile SPIR-V -> GLSL at load time), so they can be enabled independently of EZ_BUILD_VULKAN.
# On Windows and Android the toolchain is built by default (as before, when Vulkan was always built).
cmake_host_system_information(RESULT _EZ_HOST_OS QUERY OS_NAME)
if(_EZ_HOST_OS STREQUAL "Linux" OR _EZ_HOST_OS STREQUAL "Darwin")
	set(EZ_BUILD_SHADERCOMPILER_DXC OFF CACHE BOOL "Whether to build the DXC based shader compiler toolchain.")
else()
	set(EZ_BUILD_SHADERCOMPILER_DXC ON CACHE BOOL "Whether to build the DXC based shader compiler toolchain.")
endif()
unset(_EZ_HOST_OS)

# #####################################
# ## ez_requires_dxc_toolchain()
# ## Require only the DXC based shader compiler toolchain (NOT the Vulkan renderer)
# #####################################
macro(ez_requires_dxc_toolchain)
	ez_requires(EZ_CMAKE_PLATFORM_SUPPORTS_VULKAN)
	ez_requires(EZ_BUILD_SHADERCOMPILER_DXC)
	find_package(EzVulkan REQUIRED)
endmacro()

# #####################################
# ## ez_requires_vulkan()
# #####################################
macro(ez_requires_vulkan)
	ez_requires(EZ_CMAKE_PLATFORM_SUPPORTS_VULKAN)
	ez_requires(EZ_BUILD_VULKAN)
	find_package(EzVulkan REQUIRED)
endmacro()

# #####################################
# ## ez_link_target_dxc(<target>)
# #####################################
function(ez_link_target_dxc TARGET_NAME)
	ez_requires_dxc_toolchain()

	find_package(EzVulkan REQUIRED)

	if(EZVULKAN_FOUND)
		target_link_libraries(${TARGET_NAME} PRIVATE EzVulkan::DXC)

		get_target_property(_dll_location EzVulkan::DXC IMPORTED_LOCATION)

		if(NOT _dll_location STREQUAL "")
			add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:EzVulkan::DXC> $<TARGET_FILE_DIR:${TARGET_NAME}>)
		endif()

		unset(_dll_location)
	endif()
endfunction()
