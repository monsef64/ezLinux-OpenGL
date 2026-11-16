#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <RendererOpenGL/Resources/RenderTargetViewOpenGL.h>

ezGALRenderTargetViewOpenGL::ezGALRenderTargetViewOpenGL(ezGALTexture* pTexture, const ezGALRenderTargetViewCreationDescription& description)
  : ezGALRenderTargetView(pTexture, description)
{
}

ezGALRenderTargetViewOpenGL::~ezGALRenderTargetViewOpenGL() = default;

ezResult ezGALRenderTargetViewOpenGL::InitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);
  return EZ_SUCCESS;
}

ezResult ezGALRenderTargetViewOpenGL::DeInitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);
  return EZ_SUCCESS;
}
