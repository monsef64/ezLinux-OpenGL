#pragma once

#include <RendererFoundation/Shader/Shader.h>
#include <RendererOpenGL/RendererOpenGLDLL.h>

#include <glad/glad.h>

/// GL binding classes for flat binding indices.
/// Textures, texel buffers, images and samplers all consume texture units;
/// constant buffers consume GL_UNIFORM_BUFFER binding points;
/// structured / byte address buffers consume GL_SHADER_STORAGE_BUFFER binding points;
/// atomic counters consume GL_ATOMIC_COUNTER_BUFFER binding points.
enum class ezGLBindingClass : ezUInt8
{
  UniformBuffer = 0,
  StorageBuffer = 1,
  ImageUnit = 2,
  AtomicCounter = 3,
};

/// Shader object owning SPIR-V bytecode that is converted to GLSL and linked into a GL program at load time.
class EZ_RENDEREROPENGL_DLL ezGALShaderOpenGL : public ezGALShader
{
public:
  virtual void SetDebugName(ezStringView sName) const override;

  EZ_ALWAYS_INLINE GLuint GetProgram() const { return m_uiProgram; }

  /// Returns the flat GL binding index for the given resource. Returns 0xFFFFFFFF if the shader does not use this binding.
  ezUInt32 GetFlatBinding(ezGLBindingClass bindingClass, ezInt16 iBindGroup, ezInt16 iSlot) const;

protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  ezGALShaderOpenGL(const ezGALShaderCreationDescription& description);
  virtual ~ezGALShaderOpenGL();

  virtual ezResult InitPlatform(ezGALDevice* pDevice) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;

private:
  static ezUInt64 MakeFlatBindingKey(ezGLBindingClass bindingClass, ezInt16 iBindGroup, ezInt16 iSlot);

  GLuint m_uiProgram = 0;
  ezHashTable<ezUInt64, ezUInt32> m_FlatBindings;
};
