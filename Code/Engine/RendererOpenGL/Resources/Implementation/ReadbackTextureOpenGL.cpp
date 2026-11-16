#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <RendererFoundation/Resources/ResourceFormats.h>
#include <RendererOpenGL/Resources/ReadbackTextureOpenGL.h>
#include <RendererOpenGL/Resources/TextureOpenGL.h>
#include <RendererOpenGL/Utils/MappingTablesOpenGL.h>

ezGALReadbackTextureOpenGL::ezGALReadbackTextureOpenGL(const ezGALTextureCreationDescription& Description)
  : ezGALReadbackTexture(Description)
{
}

ezGALReadbackTextureOpenGL::~ezGALReadbackTextureOpenGL() = default;

ezResult ezGALReadbackTextureOpenGL::InitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);

  m_eTarget = ezGALTextureOpenGL::ToGLTarget(m_Description.m_Type, false);

  const GLenum eInternalFormat = ezMappingTables::ToGLTextureInternalFormat(m_Description.m_Format);
  if (eInternalFormat == 0)
  {
    ezLog::Error("Texture format {0} is not supported for readback textures.", m_Description.m_Format.GetValue());
    return EZ_FAILURE;
  }

  if (glCreateTextures != nullptr)
  {
    glCreateTextures(m_eTarget, 1, &m_uiTexture);
    const GLsizei uiMipLevels = static_cast<GLsizei>(m_Description.m_uiMipLevelCount);
    switch (m_Description.m_Type)
    {
      case ezGALTextureType::Texture3D:
        glTextureStorage3D(m_uiTexture, uiMipLevels, eInternalFormat, static_cast<GLsizei>(m_Description.m_uiWidth), static_cast<GLsizei>(m_Description.m_uiHeight), static_cast<GLsizei>(m_Description.m_uiDepth));
        break;
      case ezGALTextureType::Texture2DArray:
        glTextureStorage3D(m_uiTexture, uiMipLevels, eInternalFormat, static_cast<GLsizei>(m_Description.m_uiWidth), static_cast<GLsizei>(m_Description.m_uiHeight), static_cast<GLsizei>(m_Description.m_uiArraySize));
        break;
      case ezGALTextureType::TextureCube:
      case ezGALTextureType::TextureCubeArray:
        glTextureStorage3D(m_uiTexture, uiMipLevels, eInternalFormat, static_cast<GLsizei>(m_Description.m_uiWidth), static_cast<GLsizei>(m_Description.m_uiHeight), static_cast<GLsizei>(m_Description.m_uiArraySize * 6));
        break;
      default:
        glTextureStorage2D(m_uiTexture, uiMipLevels, eInternalFormat, static_cast<GLsizei>(m_Description.m_uiWidth), static_cast<GLsizei>(m_Description.m_uiHeight));
        break;
    }
  }
  else
  {
    ezLog::Error("glCreateTextures is required for the OpenGL backend.");
    return EZ_FAILURE;
  }

  if (m_uiTexture == 0)
  {
    ezLog::Error("Failed to create readback texture");
    return EZ_FAILURE;
  }

  return EZ_SUCCESS;
}

ezResult ezGALReadbackTextureOpenGL::DeInitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);

  for (void* p : m_MappedAllocations)
  {
    free(p);
  }
  m_MappedAllocations.Clear();

  if (m_uiTexture != 0)
  {
    glDeleteTextures(1, &m_uiTexture);
    m_uiTexture = 0;
  }
  return EZ_SUCCESS;
}
