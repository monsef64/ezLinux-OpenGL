#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <RendererOpenGL/Resources/SharedTextureOpenGL.h>

ezGALSharedTextureOpenGL::ezGALSharedTextureOpenGL(const ezGALTextureCreationDescription& Description, ezEnum<ezGALSharedTextureType> sharedType, ezGALPlatformSharedHandle handle)
  : ezGALTextureOpenGL(Description)
{
  m_SharedType = sharedType;
  m_hSharedHandle = handle;
}

ezGALSharedTextureOpenGL::~ezGALSharedTextureOpenGL() = default;

ezResult ezGALSharedTextureOpenGL::InitPlatform(ezGALDevice* pDevice, ezArrayPtr<ezGALSystemMemoryDescription> pInitialData)
{
  EZ_IGNORE_UNUSED(pDevice);
  EZ_IGNORE_UNUSED(pInitialData);
  ezLog::Error("Shared textures are not supported on the OpenGL backend.");
  return EZ_FAILURE;
}

ezResult ezGALSharedTextureOpenGL::DeInitPlatform(ezGALDevice* pDevice)
{
  return ezGALTextureOpenGL::DeInitPlatform(pDevice);
}

ezGALPlatformSharedHandle ezGALSharedTextureOpenGL::GetSharedHandle() const
{
  return m_hSharedHandle;
}

void ezGALSharedTextureOpenGL::WaitSemaphoreGPU(ezUInt64 uiValue) const
{
  EZ_IGNORE_UNUSED(uiValue);
}

void ezGALSharedTextureOpenGL::SignalSemaphoreGPU(ezUInt64 uiValue) const
{
  EZ_IGNORE_UNUSED(uiValue);
}
