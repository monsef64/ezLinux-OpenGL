#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <RendererOpenGL/Shader/PipelineLayoutOpenGL.h>

ezGALPipelineLayoutOpenGL::ezGALPipelineLayoutOpenGL(const ezGALPipelineLayoutCreationDescription& Description)
  : ezGALPipelineLayout(Description)
{
}

ezGALPipelineLayoutOpenGL::~ezGALPipelineLayoutOpenGL() = default;

ezResult ezGALPipelineLayoutOpenGL::InitPlatform(ezGALDevice*)
{
  return EZ_SUCCESS;
}

ezResult ezGALPipelineLayoutOpenGL::DeInitPlatform(ezGALDevice*)
{
  return EZ_SUCCESS;
}
