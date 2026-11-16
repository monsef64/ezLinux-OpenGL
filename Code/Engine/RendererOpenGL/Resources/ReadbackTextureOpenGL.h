#pragma once

#include <RendererFoundation/Resources/ReadbackTexture.h>
#include <RendererFoundation/Resources/Texture.h>
#include <RendererOpenGL/RendererOpenGLDLL.h>

#include <glad/glad.h>

/// Readback texture implemented as a GL texture object that receives a copy of the source texture via glCopyImageSubData.
class EZ_RENDEREROPENGL_DLL ezGALReadbackTextureOpenGL : public ezGALReadbackTexture
{
public:
  EZ_ALWAYS_INLINE GLuint GetGLTexture() const { return m_uiTexture; }
  EZ_ALWAYS_INLINE GLenum GetTarget() const { return m_eTarget; }

protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  ezGALReadbackTextureOpenGL(const ezGALTextureCreationDescription& Description);
  virtual ~ezGALReadbackTextureOpenGL();

  virtual ezResult InitPlatform(ezGALDevice* pDevice) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;

  virtual void SetDebugNamePlatform(const char* szName) const override { EZ_IGNORE_UNUSED(szName); }


private:
  /// Allocations made by LockTexturePlatform so UnlockTexturePlatform can free them.
  mutable ezDynamicArray<void*> m_MappedAllocations;

protected:
  GLuint m_uiTexture = 0;
  GLenum m_eTarget = GL_TEXTURE_2D;
};
