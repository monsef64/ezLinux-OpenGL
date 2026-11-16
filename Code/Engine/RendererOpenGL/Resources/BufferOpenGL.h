#pragma once

#include <RendererFoundation/Resources/Buffer.h>
#include <RendererOpenGL/RendererOpenGLDLL.h>

#include <glad/glad.h>

class ezGALDeviceOpenGL;

class EZ_RENDEREROPENGL_DLL ezGALBufferOpenGL : public ezGALBuffer
{
public:
  EZ_ALWAYS_INLINE GLuint GetGLBuffer() const { return m_uiBuffer; }
  EZ_ALWAYS_INLINE ezEnum<ezGALIndexType> GetIndexFormat() const { return m_IndexType; }

protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  ezGALBufferOpenGL(const ezGALBufferCreationDescription& Description);
  virtual ~ezGALBufferOpenGL();

  virtual ezResult InitPlatform(ezGALDevice* pDevice, ezArrayPtr<const ezUInt8> pInitialData) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;

  virtual void SetDebugNamePlatform(const char* szName) const override { EZ_IGNORE_UNUSED(szName); }


  GLuint m_uiBuffer = 0;
  ezEnum<ezGALIndexType> m_IndexType{ezGALIndexType::UInt};
};
