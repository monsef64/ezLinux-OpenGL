#pragma once

#include <RendererFoundation/Shader/VertexDeclaration.h>
#include <RendererOpenGL/RendererOpenGLDLL.h>

#include <glad/glad.h>

/// Vertex declaration translated into GL attribute state at bind time.
class EZ_RENDEREROPENGL_DLL ezGALVertexDeclarationOpenGL : public ezGALVertexDeclaration
{
public:
  struct Attribute
  {
    ezUInt32 m_uiLocation = 0;    ///< GLSL input location (from the SPIR-V reflection).
    ezUInt8 m_uiVertexBufferSlot = 0;
    ezUInt32 m_uiOffset = 0;
    GLint m_iComponents = 4;      ///< Number of components consumed from the attribute buffer.
    GLenum m_eType = GL_FLOAT;    ///< GL data type.
    GLboolean m_bNormalized = GL_FALSE;
    bool m_bInteger = false;      ///< Integer attributes (use glVertexAttribIPointer).
    ezGALVertexBindingRate::Enum m_eRate = ezGALVertexBindingRate::Vertex;
    ezUInt8 m_uiDivisor = 0;
  };

  EZ_ALWAYS_INLINE const ezHybridArray<Attribute, 8>& GetAttributes() const { return m_Attributes; }
  EZ_ALWAYS_INLINE const ezDynamicArray<ezUInt32>& GetVertexBufferStrides() const { return m_VertexBufferStrides; }

protected:
  friend class ezGALDeviceOpenGL;
  friend class ezMemoryUtils;

  ezGALVertexDeclarationOpenGL(const ezGALVertexDeclarationCreationDescription& Description);
  virtual ~ezGALVertexDeclarationOpenGL();

  virtual ezResult InitPlatform(ezGALDevice* pDevice) override;
  virtual ezResult DeInitPlatform(ezGALDevice* pDevice) override;

private:
  ezHybridArray<Attribute, 8> m_Attributes;
  ezDynamicArray<ezUInt32> m_VertexBufferStrides;
};
