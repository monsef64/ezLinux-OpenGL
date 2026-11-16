#pragma once

#include <RendererOpenGL/RendererOpenGLDLL.h>

#include <Foundation/Types/UniquePtr.h>
#include <RendererFoundation/Descriptors/Descriptors.h>
#include <RendererFoundation/Device/SwapChain.h>

#include <glad/glad.h>

class ezGALDeviceOpenGL;
struct ezGALWindowSwapChainCreationDescription;

/// Window swap-chain backed by an EGL window surface on the native window.
///
/// The actual back-buffer is a regular GL texture (created with RenderTarget | Presentable flags) that the
/// renderer draws into like any other render target. At present time the texture is blitted onto the
/// window surface's default framebuffer and eglSwapBuffers is called. This keeps all multi-sampling,
/// resolve and clear logic identical to off-screen rendering.
class EZ_RENDEREROPENGL_DLL ezGALSwapChainWindowOpenGL : public ezGALWindowSwapChain
{
public:
  virtual void AcquireNextRenderTarget(ezGALDevice* pDevice) override;
  virtual void PresentRenderTarget(ezGALDevice* pDevice) override;
  virtual ezResult UpdateSwapChain(ezGALDevice* pDevice, ezEnum<ezGALPresentMode> newPresentMode) override;

protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  ezGALSwapChainWindowOpenGL(const ezGALWindowSwapChainCreationDescription& Description);
  virtual ~ezGALSwapChainWindowOpenGL();

  virtual ezResult InitPlatform(ezGALDevice* pDevice) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;

private:
  ezResult CreateWindowSurfaceInternal();
  void DestroyBackBufferInternal(ezGALDeviceOpenGL* pDevice);
  ezResult CreateBackBufferInternal(ezGALDeviceOpenGL* pDevice);
  void ApplyPresentMode();

  ezGALDeviceOpenGL* m_pDevice = nullptr;
  void* m_pWindowSurface = nullptr; ///< EGLSurface on the native window
  GLuint m_uiBlitFBO = 0;           ///< Read FBO used to blit the back-buffer texture onto the default framebuffer

  ezGALTextureHandle m_hBackBufferTexture;

  ezEnum<ezGALPresentMode> m_CurrentPresentMode;
};
