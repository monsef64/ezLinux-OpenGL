#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <Foundation/Math/Color.h>
#include <RendererOpenGL/State/StateOpenGL.h>
#include <RendererOpenGL/Utils/MappingTablesOpenGL.h>

#include <glad/glad.h>

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

// Blend state

ezGALBlendStateOpenGL::ezGALBlendStateOpenGL(const ezGALBlendStateCreationDescription& Description)
  : ezGALBlendState(Description)
{
}

ezGALBlendStateOpenGL::~ezGALBlendStateOpenGL() = default;

ezResult ezGALBlendStateOpenGL::InitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);
  return EZ_SUCCESS;
}

ezResult ezGALBlendStateOpenGL::DeInitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);
  return EZ_SUCCESS;
}

// Depth stencil state

ezGALDepthStencilStateOpenGL::ezGALDepthStencilStateOpenGL(const ezGALDepthStencilStateCreationDescription& Description)
  : ezGALDepthStencilState(Description)
{
}

ezGALDepthStencilStateOpenGL::~ezGALDepthStencilStateOpenGL() = default;

ezResult ezGALDepthStencilStateOpenGL::InitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);
  return EZ_SUCCESS;
}

ezResult ezGALDepthStencilStateOpenGL::DeInitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);
  return EZ_SUCCESS;
}

// Rasterizer state

ezGALRasterizerStateOpenGL::ezGALRasterizerStateOpenGL(const ezGALRasterizerStateCreationDescription& Description)
  : ezGALRasterizerState(Description)
{
}

ezGALRasterizerStateOpenGL::~ezGALRasterizerStateOpenGL() = default;

ezResult ezGALRasterizerStateOpenGL::InitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);
  return EZ_SUCCESS;
}

ezResult ezGALRasterizerStateOpenGL::DeInitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);
  return EZ_SUCCESS;
}

// Sampler state

ezGALSamplerStateOpenGL::ezGALSamplerStateOpenGL(const ezGALSamplerStateCreationDescription& Description)
  : ezGALSamplerState(Description)
{
}

ezGALSamplerStateOpenGL::~ezGALSamplerStateOpenGL() = default;

ezResult ezGALSamplerStateOpenGL::InitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);

  glGenSamplers(1, reinterpret_cast<GLuint*>(&m_uiSampler));
  if (m_uiSampler == 0)
  {
    ezLog::Error("Failed to create sampler object");
    return EZ_FAILURE;
  }

  const auto& desc = m_Description;

  GLint minFilter = ezMappingTables::ToGLTexMinFilter(desc.m_MinFilter, desc.m_MipFilter);
  GLint magFilter = ezMappingTables::ToGLTexMagFilter(desc.m_MagFilter);

  glSamplerParameteri(m_uiSampler, GL_TEXTURE_MIN_FILTER, minFilter);
  glSamplerParameteri(m_uiSampler, GL_TEXTURE_MAG_FILTER, magFilter);
  glSamplerParameteri(m_uiSampler, GL_TEXTURE_WRAP_S, ezMappingTables::ToGLAddressMode(desc.m_AddressU));
  glSamplerParameteri(m_uiSampler, GL_TEXTURE_WRAP_T, ezMappingTables::ToGLAddressMode(desc.m_AddressV));
  glSamplerParameteri(m_uiSampler, GL_TEXTURE_WRAP_R, ezMappingTables::ToGLAddressMode(desc.m_AddressW));
  glSamplerParameterf(m_uiSampler, GL_TEXTURE_LOD_BIAS, desc.m_fMipLodBias);
  glSamplerParameterf(m_uiSampler, GL_TEXTURE_MIN_LOD, desc.m_fMinMip);
  glSamplerParameterf(m_uiSampler, GL_TEXTURE_MAX_LOD, desc.m_fMaxMip == 42000.0f ? 1000.0f : desc.m_fMaxMip);

  // Border color
  const ezColor border = desc.m_BorderColor;
  const float borderColor[4] = {border.r, border.g, border.b, border.a};
  glSamplerParameterfv(m_uiSampler, GL_TEXTURE_BORDER_COLOR, borderColor);

  // Comparison sampler
  if (desc.m_SampleCompareFunc != ezGALCompareFunc::Never)
  {
    glSamplerParameteri(m_uiSampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glSamplerParameteri(m_uiSampler, GL_TEXTURE_COMPARE_FUNC, ezMappingTables::ToGLCompareFunc(desc.m_SampleCompareFunc));
  }
  else
  {
    glSamplerParameteri(m_uiSampler, GL_TEXTURE_COMPARE_MODE, GL_NONE);
  }

  // Anisotropic filtering
  const float maxAnisotropy = desc.m_uiMaxAnisotropy <= 1 ? 1.0f : static_cast<float>(desc.m_uiMaxAnisotropy);
  if (desc.m_MinFilter == ezGALTextureFilterMode::Anisotropic || desc.m_MagFilter == ezGALTextureFilterMode::Anisotropic)
  {
    glSamplerParameterf(m_uiSampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);
  }

  return EZ_SUCCESS;
}

ezResult ezGALSamplerStateOpenGL::DeInitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);

  if (m_uiSampler != 0)
  {
    const GLuint uiSampler = m_uiSampler;
    glDeleteSamplers(1, &uiSampler);
    m_uiSampler = 0;
  }
  return EZ_SUCCESS;
}
