#pragma once

/// Minimal EGL loader for Linux.
///
/// libEGL is loaded dynamically at run-time, so there is no link-time and no
/// build-time dependency on EGL development packages.
///
/// Only the subset of EGL that the OpenGL backend needs is wrapped. All types and
/// constants are re-declared here, because there might not be EGL headers available
/// on the build machine. This file is only used on Linux.

#include <Foundation/Basics.h>
#include <Foundation/Strings/String.h>

#include <cstdint>

#if EZ_ENABLED(EZ_PLATFORM_LINUX)

namespace ezEGL
{
  using EGLBoolean = int;
  using EGLint = int32_t;
  using EGLenum = uint32_t;
  using EGLDisplay = void*;
  using EGLConfig = void*;
  using EGLSurface = void*;
  using EGLContext = void*;
  using EGLNativeDisplayType = void*;
  using EGLNativeWindowType = uintptr_t;
  using EGLNativePixmapType = uintptr_t;
  using EGLNativeSocketType = intptr_t;

  static constexpr EGLenum EGL_NONE = 0x3038;
  static constexpr EGLint EGL_FALSE_VALUE = 0;
  static constexpr EGLint EGL_TRUE_VALUE = 1;

  static constexpr EGLint EGL_SUCCESS = 0x3000;
  static constexpr EGLint EGL_NOT_INITIALIZED = 0x3001;
  static constexpr EGLint EGL_BAD_ACCESS = 0x3002;
  static constexpr EGLint EGL_BAD_ALLOC = 0x3003;
  static constexpr EGLint EGL_BAD_ATTRIBUTE = 0x3004;
  static constexpr EGLint EGL_BAD_CONFIG = 0x3005;
  static constexpr EGLint EGL_BAD_CONTEXT = 0x3006;
  static constexpr EGLint EGL_BAD_CURRENT_SURFACE = 0x3007;
  static constexpr EGLint EGL_BAD_DISPLAY = 0x3008;
  static constexpr EGLint EGL_BAD_MATCH = 0x3009;
  static constexpr EGLint EGL_BAD_NATIVE_PIXMAP = 0x300A;
  static constexpr EGLint EGL_BAD_NATIVE_WINDOW = 0x300B;
  static constexpr EGLint EGL_BAD_PARAMETER = 0x300C;
  static constexpr EGLint EGL_BAD_SURFACE = 0x300D;
  static constexpr EGLint EGL_CONTEXT_LOST = 0x300E;

  static constexpr EGLenum EGL_OPENGL_BIT = 0x0008;
  static constexpr EGLenum EGL_WINDOW_BIT = 0x0004;
  static constexpr EGLenum EGL_PBUFFER_BIT = 0x0001;

  static constexpr EGLint EGL_BUFFER_SIZE = 0x3020;
  static constexpr EGLint EGL_RED_SIZE = 0x3024;
  static constexpr EGLint EGL_GREEN_SIZE = 0x3023;
  static constexpr EGLint EGL_BLUE_SIZE = 0x3022;
  static constexpr EGLint EGL_ALPHA_SIZE = 0x3021;
  static constexpr EGLint EGL_DEPTH_SIZE = 0x3025;
  static constexpr EGLint EGL_STENCIL_SIZE = 0x3026;
  static constexpr EGLint EGL_SAMPLES = 0x3031;
  static constexpr EGLint EGL_SAMPLE_BUFFERS = 0x3032;
  static constexpr EGLint EGL_RENDERABLE_TYPE = 0x3040;
  static constexpr EGLint EGL_SURFACE_TYPE = 0x3033;
  static constexpr EGLint EGL_CONFIG_CAVEAT = 0x3027;
  static constexpr EGLint EGL_NONE_CONFIG = 0x3038;
  static constexpr EGLint EGL_SLOW_CONFIG = 0x3050;
  static constexpr EGLint EGL_NON_CONFORMANT_CONFIG = 0x3051;

  // EGL 1.4 context creation attributes
  static constexpr EGLint EGL_CONTEXT_CLIENT_VERSION = 0x3098;
  // EGL_KHR_create_context
  static constexpr EGLint EGL_CONTEXT_MAJOR_VERSION_KHR = 0x3098;
  static constexpr EGLint EGL_CONTEXT_MINOR_VERSION_KHR = 0x30FB;
  static constexpr EGLint EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR = 0x30FD;
  static constexpr EGLint EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR = 0x00000001;
  static constexpr EGLint EGL_CONTEXT_FLAGS_KHR = 0x30FC;
  static constexpr EGLint EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR = 0x00000001;

  // EGL_KHR_gl_colorspace
  static constexpr EGLint EGL_GL_COLORSPACE_KHR = 0x309D;
  static constexpr EGLint EGL_GL_COLORSPACE_SRGB_KHR = 0x30A9;
  static constexpr EGLint EGL_GL_COLORSPACE_LINEAR_KHR = 0x30B1;

  static constexpr EGLint EGL_WIDTH = 0x3057;
  static constexpr EGLint EGL_HEIGHT = 0x3056;

  static constexpr EGLNativeDisplayType EGL_DEFAULT_DISPLAY_HANDLE = nullptr;

  // EGL client APIs (eglBindAPI)
  static constexpr EGLenum EGL_OPENGL_API = 0x30A2;
  static constexpr EGLenum EGL_OPENGL_ES_API = 0x30A0;
  static constexpr EGLenum EGL_OPENVG_API = 0x30A1;

  /// All EGL functions that are used by the backend, loaded from libEGL.so.1.
  struct FunctionTable
  {
    EGLDisplay (*eglGetDisplay)(EGLNativeDisplayType display_id) = nullptr;
    EGLBoolean (*eglInitialize)(EGLDisplay dpy, EGLint* major, EGLint* minor) = nullptr;
    EGLBoolean (*eglBindAPI)(EGLenum api) = nullptr;
    EGLBoolean (*eglTerminate)(EGLDisplay dpy) = nullptr;
    EGLBoolean (*eglGetConfigs)(EGLDisplay dpy, EGLConfig* configs, EGLint config_size, EGLint* num_config) = nullptr;
    EGLBoolean (*eglChooseConfig)(EGLDisplay dpy, const EGLint* attrib_list, EGLConfig* configs, EGLint config_size, EGLint* num_config) = nullptr;
    EGLBoolean (*eglGetConfigAttrib)(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint* value) = nullptr;
    EGLSurface (*eglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint* attrib_list) = nullptr;
    EGLSurface (*eglCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config, const EGLint* attrib_list) = nullptr;
    EGLBoolean (*eglDestroySurface)(EGLDisplay dpy, EGLSurface surface) = nullptr;
    EGLContext (*eglCreateContext)(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint* attrib_list) = nullptr;
    EGLBoolean (*eglDestroyContext)(EGLDisplay dpy, EGLContext ctx) = nullptr;
    EGLBoolean (*eglMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) = nullptr;
    EGLBoolean (*eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface) = nullptr;
    EGLBoolean (*eglSwapInterval)(EGLDisplay dpy, EGLint interval) = nullptr;
    EGLint (*eglGetError)(void) = nullptr;
    const char* (*eglQueryString)(EGLDisplay dpy, EGLint name) = nullptr;
    void* (*eglGetProcAddress)(const char* procname) = nullptr;
    EGLSurface (*eglGetCurrentSurface)(EGLint readdraw) = nullptr;
    EGLContext (*eglGetCurrentContext)(void) = nullptr;
    EGLBoolean (*eglQuerySurface)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint* value) = nullptr;
  };

  /// Loads libEGL on first use. Returns nullptr on failure.
  /// \param[out] out_sError Set to the reason of the failure if the function returns nullptr.
  const FunctionTable* LoadFunctionTable(ezStringBuilder& out_sError);

  /// Returns the EGL error as a readable string. Must only be called after a failed EGL call.
  ezStringView GetErrorString(EGLint error);

} // namespace ezEGL

#endif
