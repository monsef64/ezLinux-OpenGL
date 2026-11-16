#pragma once

#include <RendererCore/ShaderCompiler/ShaderCompiler.h>
#include <ShaderCompilerOpenGL/ShaderCompilerOpenGLDLL.h>
#include <ShaderCompilerVulkan/ShaderCompilerVulkan.h>

class EZ_SHADERCOMPILEROPENGL_DLL ezShaderCompilerOpenGL : public ezShaderCompilerVulkan
{
  EZ_ADD_DYNAMIC_REFLECTION(ezShaderCompilerOpenGL, ezShaderCompilerVulkan);

public:
  virtual void GetSupportedPlatforms(ezHybridArray<ezString, 4>& out_platforms) override
  {
    out_platforms.PushBack("OPENGL");
  }

  virtual ezEnum<ezGALBufferLayout> GetMaterialBufferLayout(ezStringView sPlatform) const override
  {
    return ezGALBufferLayout::Vulkan_Std430_relaxed;
  }
};
