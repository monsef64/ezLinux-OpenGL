#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <Foundation/Logging/Log.h>
#include <Foundation/Strings/StringBuilder.h>
#include <RendererOpenGL/Utils/EGLLoader.h>

#if EZ_ENABLED(EZ_PLATFORM_LINUX)

#  include <dlfcn.h>
#  include <mutex>

namespace ezEGL
{
  namespace
  {
    struct EGLState
    {
      FunctionTable m_Table;
      ezStringBuilder m_sError;
      bool m_bLoaded = false;
      void* m_pLibEGL = nullptr;
    };

    EGLState& GetState()
    {
      static EGLState s_state;
      return s_state;
    }
  } // namespace

  const FunctionTable* LoadFunctionTable(ezStringBuilder& out_sError)
  {
    static std::once_flag s_once;
    auto& state = GetState();

    std::call_once(s_once, [&state]()
      {
      static const char* s_Names[] = { "libEGL.so.1", "libEGL.so" };
      for (const char* szName : s_Names)
      {
        void* pLib = dlopen(szName, RTLD_NOW | RTLD_LOCAL);
        if (pLib != nullptr)
        {
          state.m_pLibEGL = pLib;
          break;
        }
      }
      if (state.m_pLibEGL == nullptr)
      {
        state.m_sError = "Failed to load libEGL (looked for libEGL.so.1 and libEGL.so). Please install an EGL implementation, e.g. mesa.";
        return;
      }

      auto load = [&state](const char* szName) -> void*
      {
        void* pFunc = dlsym(state.m_pLibEGL, szName);
        if (pFunc == nullptr)
        {
          ezStringBuilder sb;
          sb.SetFormat("Failed to resolve EGL function '{}'", szName);
          state.m_sError.Append(sb);
        }
        return pFunc;
      };

      FunctionTable& t = state.m_Table;
      t.eglGetDisplay = reinterpret_cast<EGLDisplay (*)(EGLNativeDisplayType)>(load("eglGetDisplay"));
      t.eglInitialize = reinterpret_cast<EGLBoolean (*)(EGLDisplay, EGLint*, EGLint*)>(load("eglInitialize"));
      t.eglBindAPI = reinterpret_cast<EGLBoolean (*)(EGLenum)>(load("eglBindAPI"));
      t.eglTerminate = reinterpret_cast<EGLBoolean (*)(EGLDisplay)>(load("eglTerminate"));
      t.eglGetConfigs = reinterpret_cast<EGLBoolean (*)(EGLDisplay, EGLConfig*, EGLint, EGLint*)>(load("eglGetConfigs"));
      t.eglChooseConfig = reinterpret_cast<EGLBoolean (*)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*)>(load("eglChooseConfig"));
      t.eglGetConfigAttrib = reinterpret_cast<EGLBoolean (*)(EGLDisplay, EGLConfig, EGLint, EGLint*)>(load("eglGetConfigAttrib"));
      t.eglCreateWindowSurface = reinterpret_cast<EGLSurface (*)(EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint*)>(load("eglCreateWindowSurface"));
      t.eglCreatePbufferSurface = reinterpret_cast<EGLSurface (*)(EGLDisplay, EGLConfig, const EGLint*)>(load("eglCreatePbufferSurface"));
      t.eglDestroySurface = reinterpret_cast<EGLBoolean (*)(EGLDisplay, EGLSurface)>(load("eglDestroySurface"));
      t.eglCreateContext = reinterpret_cast<EGLContext (*)(EGLDisplay, EGLConfig, EGLContext, const EGLint*)>(load("eglCreateContext"));
      t.eglDestroyContext = reinterpret_cast<EGLBoolean (*)(EGLDisplay, EGLContext)>(load("eglDestroyContext"));
      t.eglMakeCurrent = reinterpret_cast<EGLBoolean (*)(EGLDisplay, EGLSurface, EGLSurface, EGLContext)>(load("eglMakeCurrent"));
      t.eglSwapBuffers = reinterpret_cast<EGLBoolean (*)(EGLDisplay, EGLSurface)>(load("eglSwapBuffers"));
      t.eglSwapInterval = reinterpret_cast<EGLBoolean (*)(EGLDisplay, EGLint)>(load("eglSwapInterval"));
      t.eglGetError = reinterpret_cast<EGLint (*)(void)>(load("eglGetError"));
      t.eglQueryString = reinterpret_cast<const char* (*)(EGLDisplay, EGLint)>(load("eglQueryString"));
      t.eglGetProcAddress = reinterpret_cast<void* (*)(const char*)>(load("eglGetProcAddress"));
      t.eglGetCurrentSurface = reinterpret_cast<EGLSurface (*)(EGLint)>(load("eglGetCurrentSurface"));
      t.eglGetCurrentContext = reinterpret_cast<EGLContext (*)(void)>(load("eglGetCurrentContext"));
      t.eglQuerySurface = reinterpret_cast<EGLBoolean (*)(EGLDisplay, EGLSurface, EGLint, EGLint*)>(load("eglQuerySurface"));

      if (!state.m_sError.IsEmpty())
      {
        dlclose(state.m_pLibEGL);
        state.m_pLibEGL = nullptr;
        return;
      }
      state.m_bLoaded = true; });

    if (!state.m_bLoaded)
    {
      out_sError.Set(state.m_sError);
      return nullptr;
    }
    return &state.m_Table;
  }

  ezStringView GetErrorString(EGLint error)
  {
    switch (error)
    {
      case EGL_SUCCESS: return "EGL_SUCCESS";
      case EGL_NOT_INITIALIZED: return "EGL_NOT_INITIALIZED";
      case EGL_BAD_ACCESS: return "EGL_BAD_ACCESS";
      case EGL_BAD_ALLOC: return "EGL_BAD_ALLOC";
      case EGL_BAD_ATTRIBUTE: return "EGL_BAD_ATTRIBUTE";
      case EGL_BAD_CONFIG: return "EGL_BAD_CONFIG";
      case EGL_BAD_CONTEXT: return "EGL_BAD_CONTEXT";
      case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
      case EGL_BAD_DISPLAY: return "EGL_BAD_DISPLAY";
      case EGL_BAD_MATCH: return "EGL_BAD_MATCH";
      case EGL_BAD_NATIVE_PIXMAP: return "EGL_BAD_NATIVE_PIXMAP";
      case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
      case EGL_BAD_PARAMETER: return "EGL_BAD_PARAMETER";
      case EGL_BAD_SURFACE: return "EGL_BAD_SURFACE";
      case EGL_CONTEXT_LOST: return "EGL_CONTEXT_LOST";
      default: return "<unknown EGL error>";
    }
  }

} // namespace ezEGL

#endif
