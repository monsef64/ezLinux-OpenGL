# #####################################
# ## OpenGL support (Linux only for now)
# #####################################

# OpenGL is enabled by default on Linux, it is disabled by default elsewhere
# as the implementation is currently Linux specific (EGL + GLFW / X11).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(EZ_BUILD_OPENGL_DEFAULT ON)
else()
	set(EZ_BUILD_OPENGL_DEFAULT OFF)
endif()

set(EZ_BUILD_OPENGL ${EZ_BUILD_OPENGL_DEFAULT} CACHE BOOL "Whether to build OpenGL renderer support.")

# #####################################
# ## ez_requires_opengl()
# #####################################
macro(ez_requires_opengl)
	ez_requires(EZ_CMAKE_PLATFORM_SUPPORTS_OPENGL)
	ez_requires(EZ_BUILD_OPENGL)
endmacro()
