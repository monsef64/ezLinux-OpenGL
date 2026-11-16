#pragma once

#include <RendererFoundation/Resources/Texture.h>
#include <RendererOpenGL/RendererOpenGLDLL.h>

#include <glad/glad.h>

class ezGALDeviceOpenGL;

class EZ_RENDEREROPENGL_DLL ezGALTextureOpenGL : public ezGALTexture
{
public:
  EZ_ALWAYS_INLINE GLuint GetGLTexture() const { return m_uiTexture; }
  EZ_ALWAYS_INLINE GLenum GetTarget() const { return m_eTarget; }

  /// Returns the GL target for the given texture description (incl. MSAA).
  static GLenum ToGLTarget(ezGALTextureType::Enum type, bool bMSAA);

  /// Uploads a sub-region (in texel coordinates) of a mip level / array slice from client memory.
  /// For cube maps m_uiArraySlice is the face index. Compressed formats only support full-mip uploads from the origin.
  ezResult UploadSubImage(const ezGALTextureSubresource& destinationSubResource, const ezBoundingBoxu32& destinationBox, const ezGALSystemMemoryDescription& sourceData) const;

protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  ezGALTextureOpenGL(const ezGALTextureCreationDescription& Description);
  virtual ~ezGALTextureOpenGL();

  virtual ezResult InitPlatform(ezGALDevice* pDevice, ezArrayPtr<ezGALSystemMemoryDescription> pInitialData) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;

  virtual void SetDebugNamePlatform(const char* szName) const override { EZ_IGNORE_UNUSED(szName); }


private:
  ezResult CreateTextureStorage();
  ezResult UploadInitialData(ezArrayPtr<ezGALSystemMemoryDescription> pInitialData);

protected:
  GLuint m_uiTexture = 0;
  GLenum m_eTarget = GL_TEXTURE_2D;
  ezUInt32 m_uiMipLevels = 1;
  ezUInt32 m_uiArraySize = 1; ///< Number of array slices (faces for cubes: arraySize * 6).
  bool m_bMSAA = false;
};
