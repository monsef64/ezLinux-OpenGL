#pragma once

#include <RendererFoundation/Resources/RenderTargetView.h>
#include <RendererOpenGL/RendererOpenGLDLL.h>

/// OpenGL has no view objects; a render target view is a lightweight reference to a texture sub-resource that is consumed when an FBO is bound.
class EZ_RENDEREROPENGL_DLL ezGALRenderTargetViewOpenGL : public ezGALRenderTargetView
{
public:
protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  ezGALRenderTargetViewOpenGL(ezGALTexture* pTexture, const ezGALRenderTargetViewCreationDescription& description);
  virtual ~ezGALRenderTargetViewOpenGL();

  virtual ezResult InitPlatform(ezGALDevice* pDevice) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;
};
