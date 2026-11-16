#pragma once

#include <RendererOpenGL/RendererOpenGLDLL.h>
#include <RendererFoundation/State/State.h>

class ezGALDeviceOpenGL;

/// GL state objects are applied at bind time; they carry no GPU objects (except the sampler state).
class EZ_RENDEREROPENGL_DLL ezGALBlendStateOpenGL : public ezGALBlendState
{
public:
protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  ezGALBlendStateOpenGL(const ezGALBlendStateCreationDescription& Description);
  ~ezGALBlendStateOpenGL();

  virtual ezResult InitPlatform(ezGALDevice* pDevice) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;
};

class EZ_RENDEREROPENGL_DLL ezGALDepthStencilStateOpenGL : public ezGALDepthStencilState
{
public:
protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  ezGALDepthStencilStateOpenGL(const ezGALDepthStencilStateCreationDescription& Description);
  ~ezGALDepthStencilStateOpenGL();

  virtual ezResult InitPlatform(ezGALDevice* pDevice) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;
};

class EZ_RENDEREROPENGL_DLL ezGALRasterizerStateOpenGL : public ezGALRasterizerState
{
public:
protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  ezGALRasterizerStateOpenGL(const ezGALRasterizerStateCreationDescription& Description);
  ~ezGALRasterizerStateOpenGL();

  virtual ezResult InitPlatform(ezGALDevice* pDevice) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;
};

class EZ_RENDEREROPENGL_DLL ezGALSamplerStateOpenGL : public ezGALSamplerState
{
public:
  EZ_ALWAYS_INLINE void SetDebugNamePlatform(const char* szName) const override
  {
    EZ_IGNORE_UNUSED(szName);
  }

  EZ_ALWAYS_INLINE unsigned int GetSampler() const { return m_uiSampler; }

protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  ezGALSamplerStateOpenGL(const ezGALSamplerStateCreationDescription& Description);
  ~ezGALSamplerStateOpenGL();

  virtual ezResult InitPlatform(ezGALDevice* pDevice) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;

  unsigned int m_uiSampler = 0;
};
