#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <RendererOpenGL/Shader/BindGroupLayoutOpenGL.h>

ezGALBindGroupLayoutOpenGL::ezGALBindGroupLayoutOpenGL(const ezGALBindGroupLayoutCreationDescription& Description)
  : ezGALBindGroupLayout(Description)
{
}

ezGALBindGroupLayoutOpenGL::~ezGALBindGroupLayoutOpenGL() = default;

ezResult ezGALBindGroupLayoutOpenGL::InitPlatform(ezGALDevice*)
{
  return EZ_SUCCESS;
}

ezResult ezGALBindGroupLayoutOpenGL::DeInitPlatform(ezGALDevice*)
{
  return EZ_SUCCESS;
}
