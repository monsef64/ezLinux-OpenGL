#pragma once

#include <RendererOpenGL/RendererOpenGLDLL.h>
#include <RendererFoundation/RendererFoundationDLL.h>
#include <RendererFoundation/Shader/PipelineLayout.h>

class EZ_RENDEREROPENGL_DLL ezGALPipelineLayoutOpenGL : public ezGALPipelineLayout
{
public:
protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  virtual ezResult InitPlatform(ezGALDevice* pDevice) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;

  ezGALPipelineLayoutOpenGL(const ezGALPipelineLayoutCreationDescription& Description);
  virtual ~ezGALPipelineLayoutOpenGL();
};
