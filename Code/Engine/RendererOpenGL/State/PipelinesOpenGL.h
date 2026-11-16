#pragma once

#include <Foundation/Basics.h>
#include <RendererFoundation/State/ComputePipeline.h>
#include <RendererFoundation/State/GraphicsPipeline.h>
#include <RendererOpenGL/RendererOpenGLDLL.h>

class ezGALDeviceOpenGL;

class EZ_RENDEREROPENGL_DLL ezGALGraphicsPipelineOpenGL : public ezGALGraphicsPipeline
{
public:
  ezGALGraphicsPipelineOpenGL(const ezGALGraphicsPipelineCreationDescription& description);
  ~ezGALGraphicsPipelineOpenGL();

  virtual ezResult InitPlatform(ezGALDevice* pDevice) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;
  virtual void SetDebugName(const char* szName) override;
};

class EZ_RENDEREROPENGL_DLL ezGALComputePipelineOpenGL : public ezGALComputePipeline
{
public:
  ezGALComputePipelineOpenGL(const ezGALComputePipelineCreationDescription& description);
  ~ezGALComputePipelineOpenGL();

  virtual ezResult InitPlatform(ezGALDevice* pDevice) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;
  virtual void SetDebugName(const char* szName) override;
};
