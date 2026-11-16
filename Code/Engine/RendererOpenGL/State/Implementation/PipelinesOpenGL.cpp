#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <RendererOpenGL/State/PipelinesOpenGL.h>

ezGALGraphicsPipelineOpenGL::ezGALGraphicsPipelineOpenGL(const ezGALGraphicsPipelineCreationDescription& description)
  : ezGALGraphicsPipeline(description)
{
}

ezGALGraphicsPipelineOpenGL::~ezGALGraphicsPipelineOpenGL() = default;

ezResult ezGALGraphicsPipelineOpenGL::InitPlatform(ezGALDevice*)
{
  return EZ_SUCCESS;
}

ezResult ezGALGraphicsPipelineOpenGL::DeInitPlatform(ezGALDevice*)
{
  return EZ_SUCCESS;
}

void ezGALGraphicsPipelineOpenGL::SetDebugName(const char*)
{
}

ezGALComputePipelineOpenGL::ezGALComputePipelineOpenGL(const ezGALComputePipelineCreationDescription& description)
  : ezGALComputePipeline(description)
{
}

ezGALComputePipelineOpenGL::~ezGALComputePipelineOpenGL() = default;

ezResult ezGALComputePipelineOpenGL::InitPlatform(ezGALDevice*)
{
  return EZ_SUCCESS;
}

ezResult ezGALComputePipelineOpenGL::DeInitPlatform(ezGALDevice*)
{
  return EZ_SUCCESS;
}

void ezGALComputePipelineOpenGL::SetDebugName(const char*)
{
}
