#pragma once

#include <RendererFoundation/Resources/ReadbackBuffer.h>
#include <RendererOpenGL/RendererOpenGLDLL.h>

#include <glad/glad.h>

/// Readback buffer implemented as a GL buffer object with GL_MAP_READ_BIT that is mapped while the GPU reads are pending.
class EZ_RENDEREROPENGL_DLL ezGALReadbackBufferOpenGL : public ezGALReadbackBuffer
{
public:
  EZ_ALWAYS_INLINE GLuint GetGLBuffer() const { return m_uiBuffer; }

protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  ezGALReadbackBufferOpenGL(const ezGALBufferCreationDescription& Description);
  virtual ~ezGALReadbackBufferOpenGL();

  virtual ezResult InitPlatform(ezGALDevice* pDevice) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;

  virtual void SetDebugNamePlatform(const char* szName) const override { EZ_IGNORE_UNUSED(szName); }


protected:
  GLuint m_uiBuffer = 0;
};
