#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <Core/System/Window.h>
#include <Foundation/Profiling/Profiling.h>
#include <Foundation/Reflection/ReflectionUtils.h>
#include <RendererFoundation/RendererReflection.h>
#include <RendererFoundation/Resources/RenderTargetView.h>
#include <RendererOpenGL/Device/DeviceOpenGL.h>
#include <RendererOpenGL/Device/SwapChainOpenGL.h>
#include <RendererOpenGL/Resources/TextureOpenGL.h>
#include <RendererOpenGL/Utils/EGLLoader.h>

#if EZ_ENABLED(EZ_SUPPORTS_GLFW)
#  define GLFW_EXPOSE_NATIVE_X11
#  include <GLFW/glfw3.h>
#  include <GLFW/glfw3native.h>
#endif

ezGALSwapChainWindowOpenGL::ezGALSwapChainWindowOpenGL(const ezGALWindowSwapChainCreationDescription& Description)
  : ezGALWindowSwapChain(Description)
{
}

ezGALSwapChainWindowOpenGL::~ezGALSwapChainWindowOpenGL() = default;

void ezGALSwapChainWindowOpenGL::AcquireNextRenderTarget(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);
  // The back-buffer is a regular GL texture; nothing to acquire.
}

void ezGALSwapChainWindowOpenGL::PresentRenderTarget(ezGALDevice* pDevice)
{
  if (m_pWindowSurface == nullptr)
    return;
  if (m_RenderTargets.m_hRTs[0].IsInvalidated())
    return;

  ezGALDeviceOpenGL* pGLDevice = static_cast<ezGALDeviceOpenGL*>(pDevice);
  const ezEGL::FunctionTable* egl = pGLDevice->GetEGLFunctionTable();
  void* pDisplay = pGLDevice->GetEGLDisplay();

  const ezGALTexture* pTexture = pGLDevice->GetTexture(m_hBackBufferTexture);
  if (pTexture == nullptr)
  {
    ezLog::Error("Swapchain back-buffer texture is invalid, cannot present");
    return;
  }

  const ezGALTextureOpenGL* pGLTexture = static_cast<const ezGALTextureOpenGL*>(pTexture);
  const ezGALTextureCreationDescription& texDesc = pTexture->GetDescription();

  // Get the current size of the window surface, it might not match the back-buffer when the window was resized.
  ezEGL::EGLint iSurfaceWidth = 0, iSurfaceHeight = 0;
  if (egl->eglQuerySurface(pDisplay, m_pWindowSurface, ezEGL::EGL_WIDTH, &iSurfaceWidth) == 0 ||
      egl->eglQuerySurface(pDisplay, m_pWindowSurface, ezEGL::EGL_HEIGHT, &iSurfaceHeight) == 0 ||
      iSurfaceWidth <= 0 || iSurfaceHeight <= 0)
  {
    ezLog::Warning("Could not query the EGL window surface size, presenting with the back-buffer size");
    iSurfaceWidth = static_cast<ezEGL::EGLint>(texDesc.m_uiWidth);
    iSurfaceHeight = static_cast<ezEGL::EGLint>(texDesc.m_uiHeight);
  }

  if (!pGLDevice->MakeWindowSurfaceCurrent(m_pWindowSurface))
  {
    ezLog::Error("Failed to make the swapchain surface current for presentation");
    pGLDevice->MakeDeviceContextCurrent();
    return;
  }

  if (m_uiBlitFBO == 0)
  {
    glCreateFramebuffers(1, &m_uiBlitFBO);
  }

  // Attach the back-buffer texture to the read FBO and blit it onto the window's default framebuffer.
  glNamedFramebufferTexture(m_uiBlitFBO, GL_COLOR_ATTACHMENT0, pGLTexture->GetGLTexture(), 0);

  GLboolean hadScissor = GL_FALSE;
  glGetBooleanv(GL_SCISSOR_TEST, &hadScissor);
  glDisable(GL_SCISSOR_TEST);

  glBlitNamedFramebuffer(m_uiBlitFBO, 0,
    0, 0, static_cast<GLint>(texDesc.m_uiWidth), static_cast<GLint>(texDesc.m_uiHeight),
    0, 0, iSurfaceWidth, iSurfaceHeight,
    GL_COLOR_BUFFER_BIT, GL_NEAREST);

  if (hadScissor)
  {
    glEnable(GL_SCISSOR_TEST);
  }

  if (egl->eglSwapBuffers(pDisplay, m_pWindowSurface) == 0)
  {
    ezLog::Error("eglSwapBuffers failed: {0}", ezStringView(ezEGL::GetErrorString(egl->eglGetError())));
  }

  // Restore the pbuffer surface for the device context.
  pGLDevice->MakeDeviceContextCurrent();
}

ezResult ezGALSwapChainWindowOpenGL::UpdateSwapChain(ezGALDevice* pDevice, ezEnum<ezGALPresentMode> newPresentMode)
{
  m_CurrentPresentMode = newPresentMode;

  ezGALDeviceOpenGL* pGLDevice = static_cast<ezGALDeviceOpenGL*>(pDevice);

  DestroyBackBufferInternal(pGLDevice);
  if (CreateBackBufferInternal(pGLDevice).Failed())
  {
    ezLog::Error("Failed to recreate the swapchain back-buffer");
    return EZ_FAILURE;
  }

  ApplyPresentMode();
  return EZ_SUCCESS;
}

ezResult ezGALSwapChainWindowOpenGL::InitPlatform(ezGALDevice* pDevice)
{
  m_pDevice = static_cast<ezGALDeviceOpenGL*>(pDevice);
  m_CurrentPresentMode = m_WindowDesc.m_InitialPresentMode;

  if (m_WindowDesc.m_SampleCount != ezGALMSAASampleCount::None)
  {
    ezLog::Warning("Multi-sampling is not supported for OpenGL swapchains; the back-buffer is rendered at 1x.");
  }

  EZ_SUCCEED_OR_RETURN(CreateWindowSurfaceInternal());

  // The window must not be destroyed while the EGL surface is alive.
  m_WindowDesc.m_pWindow->AddReference();

  // Sets the vsync interval while the window surface is current.
  if (!m_pDevice->MakeWindowSurfaceCurrent(m_pWindowSurface))
  {
    ezLog::Error("Failed to make the swapchain surface current");
    m_pDevice->MakeDeviceContextCurrent();
    return EZ_FAILURE;
  }
  ApplyPresentMode();
  m_pDevice->MakeDeviceContextCurrent();

  if (CreateBackBufferInternal(m_pDevice).Failed())
  {
    return EZ_FAILURE;
  }

  return EZ_SUCCESS;
}

ezResult ezGALSwapChainWindowOpenGL::DeInitPlatform(ezGALDevice* pDevice)
{
  ezGALDeviceOpenGL* pGLDevice = static_cast<ezGALDeviceOpenGL*>(pDevice);

  if (m_uiBlitFBO != 0)
  {
    glDeleteFramebuffers(1, &m_uiBlitFBO);
    m_uiBlitFBO = 0;
  }

  DestroyBackBufferInternal(pGLDevice);

  const ezEGL::FunctionTable* egl = pGLDevice->GetEGLFunctionTable();
  if (m_pWindowSurface != nullptr && egl != nullptr)
  {
    egl->eglMakeCurrent(pGLDevice->GetEGLDisplay(), nullptr, nullptr, nullptr);
    egl->eglDestroySurface(pGLDevice->GetEGLDisplay(), m_pWindowSurface);
    m_pWindowSurface = nullptr;
    pGLDevice->MakeDeviceContextCurrent();
  }

  if (m_WindowDesc.m_pWindow != nullptr)
  {
    m_WindowDesc.m_pWindow->RemoveReference();
  }

  m_pDevice = nullptr;
  return EZ_SUCCESS;
}

ezResult ezGALSwapChainWindowOpenGL::CreateWindowSurfaceInternal()
{
  const ezEGL::FunctionTable* egl = m_pDevice->GetEGLFunctionTable();
  void* pDisplay = m_pDevice->GetEGLDisplay();

  ezEGL::EGLNativeWindowType nativeWindow = 0;
#if EZ_ENABLED(EZ_PLATFORM_LINUX) && EZ_ENABLED(EZ_SUPPORTS_GLFW)
  ezWindowHandle windowHandle = m_WindowDesc.m_pWindow->GetNativeWindowHandle();
  switch (windowHandle.type)
  {
    case ezWindowHandle::Type::GLFW:
    {
#  if EZ_ENABLED(EZ_SUPPORTS_GLFW)
      nativeWindow = static_cast<ezEGL::EGLNativeWindowType>(reinterpret_cast<uintptr_t>(glfwGetX11Window(windowHandle.glfwWindow)));
      if (nativeWindow == 0)
      {
        ezLog::Error("Failed to get the X11 window for the swapchain window");
        return EZ_FAILURE;
      }
#  else
      ezLog::Error("GLFW window handle but the build does not support GLFW");
      return EZ_FAILURE;
#  endif
    }
    break;
    case ezWindowHandle::Type::XCB:
      ezLog::Error("The OpenGL backend does not support XCB windows yet (it requires the EGL X11 or XCB platform integration). Use GLFW windows.");
      return EZ_FAILURE;
    default:
      ezLog::Error("Invalid native window handle for window \"{}\"", m_WindowDesc.m_pWindow);
      return EZ_FAILURE;
  }
#elif EZ_ENABLED(EZ_PLATFORM_LINUX)
  ezLog::Error("The OpenGL backend requires a window that exposes a native window handle. On Linux this means the build must have GLFW support (EZ_SUPPORTS_GLFW).");
  return EZ_FAILURE;
#else
  ezLog::Error("The OpenGL backend only supports Linux/EGL window surfaces currently");
  return EZ_FAILURE;
#endif

  // Pick a config for the window surface. Only the color buffer is needed, since we never render
  // into the default framebuffer except for the final blit, and depth/stencil are allocated as FBOs.
  auto tryCreateSurface = [&](const ezEGL::EGLint* attribs, ezStringView sDescription) -> void* {
    ezEGL::EGLConfig configs[16];
    ezEGL::EGLint iCount = 0;
    if (egl->eglChooseConfig(pDisplay, attribs, reinterpret_cast<ezEGL::EGLConfig*>(configs), 16, &iCount) == 0 || iCount == 0)
    {
      return nullptr;
    }

    // Try every matching config; the window's visual might not match the first one (EGL_BAD_MATCH).
    for (ezEGL::EGLint i = 0; i < iCount; i++)
    {
      void* pSurface = egl->eglCreateWindowSurface(pDisplay, configs[i], nativeWindow, nullptr);
      if (pSurface != nullptr)
      {
        ezLog::Debug("Created EGL window surface for the swapchain ({0}, config {1} of {2})", sDescription, i + 1, iCount);
        return pSurface;
      }
    }
    return nullptr;
  };

  const ezEGL::EGLint attribsMatch[] = {
    ezEGL::EGL_RED_SIZE, 8,
    ezEGL::EGL_GREEN_SIZE, 8,
    ezEGL::EGL_BLUE_SIZE, 8,
    ezEGL::EGL_ALPHA_SIZE, 8,
    ezEGL::EGL_SURFACE_TYPE, ezEGL::EGL_WINDOW_BIT,
    ezEGL::EGL_RENDERABLE_TYPE, ezEGL::EGL_OPENGL_BIT,
    ezEGL::EGL_NONE,
  };

  m_pWindowSurface = tryCreateSurface(attribsMatch, ezStringView("8/8/8/8"));

  if (m_pWindowSurface == nullptr)
  {
    // Fall back to any OpenGL-capable window config the driver offers.
    const ezEGL::EGLint attribsAny[] = {
      ezEGL::EGL_SURFACE_TYPE, ezEGL::EGL_WINDOW_BIT,
      ezEGL::EGL_RENDERABLE_TYPE, ezEGL::EGL_OPENGL_BIT,
      ezEGL::EGL_NONE,
    };
    m_pWindowSurface = tryCreateSurface(attribsAny, ezStringView("any"));
  }

  if (m_pWindowSurface == nullptr)
  {
    ezLog::Error("Failed to create an EGL window surface for the swapchain (last error: {0})", ezStringView(ezEGL::GetErrorString(egl->eglGetError())));
    return EZ_FAILURE;
  }

  return EZ_SUCCESS;
}

void ezGALSwapChainWindowOpenGL::ApplyPresentMode()
{
  const ezEGL::FunctionTable* egl = m_pDevice->GetEGLFunctionTable();
  if (egl == nullptr)
    return;

  // eglSwapInterval applies to the current context's draw surface; make sure the window surface is current.
  void* pOldSurface = m_pDevice->GetEGLFunctionTable()->eglGetCurrentSurface(0x305B /* EGL_DRAW */);
  const bool bWindowSurfaceWasCurrent = pOldSurface == m_pWindowSurface;
  if (!bWindowSurfaceWasCurrent)
  {
    if (!m_pDevice->MakeWindowSurfaceCurrent(m_pWindowSurface))
      return;
  }

  egl->eglSwapInterval(m_pDevice->GetEGLDisplay(), m_CurrentPresentMode == ezGALPresentMode::VSync ? 1 : 0);

  if (!bWindowSurfaceWasCurrent)
  {
    m_pDevice->MakeDeviceContextCurrent();
  }
}

void ezGALSwapChainWindowOpenGL::DestroyBackBufferInternal(ezGALDeviceOpenGL* pDevice)
{
  if (!m_hBackBufferTexture.IsInvalidated())
  {
    pDevice->DestroyTexture(m_hBackBufferTexture);
    m_hBackBufferTexture = ezGALTextureHandle();
  }
  m_RenderTargets.m_hRTs[0] = ezGALTextureHandle();
  m_RenderTargets.m_hDSTarget = ezGALTextureHandle();
}

ezResult ezGALSwapChainWindowOpenGL::CreateBackBufferInternal(ezGALDeviceOpenGL* pDevice)
{
  const ezSizeU32 size = m_WindowDesc.m_pWindow->GetClientAreaSize();

  ezGALTextureCreationDescription TexDesc;
  TexDesc.m_Type = ezGALTextureType::Texture2D;
  TexDesc.m_Format = m_WindowDesc.m_BackBufferFormat;
  TexDesc.m_uiWidth = size.width;
  TexDesc.m_uiHeight = size.height;
  TexDesc.m_SampleCount = ezGALMSAASampleCount::None; // back-buffer is always single-sampled (see InitPlatform)
  TexDesc.m_pExisitingNativeObject = nullptr;
  TexDesc.m_TextureFlags = ezGALTextureUsageFlags::RenderTarget | ezGALTextureUsageFlags::ShaderResource | ezGALTextureUsageFlags::Presentable;
  TexDesc.m_ResourceAccess.m_bImmutable = false;

  m_hBackBufferTexture = pDevice->CreateTexture(TexDesc);
  if (m_hBackBufferTexture.IsInvalidated())
  {
    ezLog::Error("Failed to create the swapchain back-buffer texture");
    return EZ_FAILURE;
  }

  m_RenderTargets.m_hRTs[0] = m_hBackBufferTexture;
  m_CurrentSize = size;
  m_pDevice->s_SwapChainUpdatedEvent.Broadcast(this);
  return EZ_SUCCESS;
}
