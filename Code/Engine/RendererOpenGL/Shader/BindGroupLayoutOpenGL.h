#pragma once

#include <RendererOpenGL/RendererOpenGLDLL.h>
#include <RendererFoundation/RendererFoundationDLL.h>
#include <RendererFoundation/Shader/BindGroupLayout.h>

class EZ_RENDEREROPENGL_DLL ezGALBindGroupLayoutOpenGL : public ezGALBindGroupLayout
{
public:
protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  virtual ezResult InitPlatform(ezGALDevice* pDevice) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;

  ezGALBindGroupLayoutOpenGL(const ezGALBindGroupLayoutCreationDescription& Description);
  virtual ~ezGALBindGroupLayoutOpenGL();
};
