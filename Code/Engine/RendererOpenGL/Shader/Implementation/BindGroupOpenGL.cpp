#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <RendererOpenGL/Shader/BindGroupOpenGL.h>

ezGALBindGroupOpenGL::ezGALBindGroupOpenGL(const ezGALBindGroupCreationDescription& Description)
  : ezGALBindGroup(Description)
{
}

ezGALBindGroupOpenGL::~ezGALBindGroupOpenGL() = default;

ezResult ezGALBindGroupOpenGL::InitPlatform(ezGALDevice*)
{
  m_bInvalidated = false;
  return EZ_SUCCESS;
}

ezResult ezGALBindGroupOpenGL::DeInitPlatform(ezGALDevice*)
{
  return EZ_SUCCESS;
}

void ezGALBindGroupOpenGL::Invalidate(ezGALDevice*)
{
  // GL bindings are bound directly at draw time, there is no cached descriptor set to reclaim.
  m_bInvalidated = true;
}

bool ezGALBindGroupOpenGL::IsInvalidated() const
{
  return m_bInvalidated;
}
