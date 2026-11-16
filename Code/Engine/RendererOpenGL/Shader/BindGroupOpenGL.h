#pragma once

#include <RendererOpenGL/RendererOpenGLDLL.h>
#include <RendererFoundation/RendererFoundationDLL.h>
#include <RendererFoundation/Shader/BindGroup.h>

class EZ_RENDEREROPENGL_DLL ezGALBindGroupOpenGL : public ezGALBindGroup
{
public:
  virtual bool IsInvalidated() const override;

protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  virtual ezResult InitPlatform(ezGALDevice* pDevice) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;

  virtual void Invalidate(ezGALDevice* pDevice) override;

  virtual void SetDebugNamePlatform(const char* szName) const override { EZ_IGNORE_UNUSED(szName); }

  ezGALBindGroupOpenGL(const ezGALBindGroupCreationDescription& Description);
  virtual ~ezGALBindGroupOpenGL();

private:
  bool m_bInvalidated = false;
};
