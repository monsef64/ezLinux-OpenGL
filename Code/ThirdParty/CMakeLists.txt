set_property(GLOBAL PROPERTY "GATHER_EXPORT_PROJECTS" TRUE)

# SPIRV-Cross is only needed by the OpenGL renderer (SPIR-V -> GLSL at run-time).
# Limit the build to the core library and the GLSL backend and disable everything else,
# before the subdirectory is added below.
if(EZ_BUILD_OPENGL)
	# NOTE: CMAKE_POLICY_VERSION_MINIMUM sets CMP0077 to OLD, so a plain 'set(...)' of an option()
	# variable in SPIRV-Cross does not survive the option() call there. Use cache FORCE variables instead.
	set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
	set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
	set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
	set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
	set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
	set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
	set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
	set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
	set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
	# SPIRV-Cross is linked into the ezRendererOpenGL shared library, so it must be built with -fPIC.
	set(SPIRV_CROSS_FORCE_PIC ON CACHE BOOL "" FORCE)
endif()

ez_add_all_subdirs()
set_property(GLOBAL PROPERTY "GATHER_EXPORT_PROJECTS" FALSE)
