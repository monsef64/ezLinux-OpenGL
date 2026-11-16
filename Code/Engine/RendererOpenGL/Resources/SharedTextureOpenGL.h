#pragma once

#include <RendererFoundation/Resources/Texture.h>
#include <RendererOpenGL/Resources/TextureOpenGL.h>
#include <RendererOpenGL/RendererOpenGLDLL.h>

/// Shared textures are not supported on the OpenGL backend (no cross-process memory objects on Linux/EGL).
class EZ_RENDEREROPENGL_DLL ezGALSharedTextureOpenGL : public ezGALTextureOpenGL, public ezGALSharedTexture
{
  using SUPER = ezGALTextureOpenGL;

protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  ezGALSharedTextureOpenGL(const ezGALTextureCreationDescription& Description, ezEnum<ezGALSharedTextureType> sharedType, ezGALPlatformSharedHandle handle);
  ~ezGALSharedTextureOpenGL();

  virtual ezResult InitPlatform(ezGALDevice* pDevice, ezArrayPtr<ezGALSystemMemoryDescription> pInitialData) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;

  virtual ezGALPlatformSharedHandle GetSharedHandle() const override;
  virtual void WaitSemaphoreGPU(ezUInt64 uiValue) const override;
  virtual void SignalSemaphoreGPU(ezUInt64 uiValue) const override;

protected:
  ezEnum<ezGALSharedTextureType> m_SharedType = ezGALSharedTextureType::None;
  ezGALPlatformSharedHandle m_hSharedHandle;
};
