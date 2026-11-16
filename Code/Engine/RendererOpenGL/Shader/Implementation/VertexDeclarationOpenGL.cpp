#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <RendererFoundation/Shader/Shader.h>
#include <RendererOpenGL/Device/DeviceOpenGL.h>
#include <RendererOpenGL/Shader/VertexDeclarationOpenGL.h>
#include <RendererOpenGL/Utils/MappingTablesOpenGL.h>

#include <glad/glad.h>

ezGALVertexDeclarationOpenGL::ezGALVertexDeclarationOpenGL(const ezGALVertexDeclarationCreationDescription& Description)
  : ezGALVertexDeclaration(Description)
{
}

ezGALVertexDeclarationOpenGL::~ezGALVertexDeclarationOpenGL() = default;

ezResult ezGALVertexDeclarationOpenGL::InitPlatform(ezGALDevice* pDevice)
{
  const ezGALShader* pShader = pDevice->GetShader(m_Description.m_hShader);

  if (pShader == nullptr || !pShader->GetDescription().HasByteCodeForStage(ezGALShaderStage::VertexShader))
  {
    return EZ_FAILURE;
  }

  auto usedVertexAttributes = pShader->GetVertexInputAttributes();

  m_VertexBufferStrides.SetCount(m_Description.m_VertexBindings.GetCount());
  for (ezUInt32 i = 0; i < m_Description.m_VertexBindings.GetCount(); i++)
  {
    m_VertexBufferStrides[i] = m_Description.m_VertexBindings[i].m_uiStride;
  }

  for (ezUInt32 i = 0; i < m_Description.m_VertexAttributes.GetCount(); i++)
  {
    const ezGALVertexAttribute& current = m_Description.m_VertexAttributes[i];

    const ezShaderVertexInputAttribute* pUsed = nullptr;
    for (const auto& attrib : usedVertexAttributes)
    {
      if (attrib.m_eSemantic == current.m_eSemantic)
      {
        pUsed = &attrib;
        break;
      }
    }

    if (pUsed == nullptr)
      continue; // attribute is not consumed by the shader

    GLint components = 0;
    GLenum type = GL_FLOAT;
    GLboolean normalized = GL_FALSE;
    bool integer = false;
    if (!ezMappingTables::ToGLVertexAttributeFormat(current.m_eFormat, components, type, normalized, integer))
    {
      ezLog::Error("Vertex attribute format {0} of attribute at index {1} is not supported by the OpenGL backend!", static_cast<ezUInt32>(current.m_eFormat), i);
      return EZ_FAILURE;
    }

    const ezGALVertexBinding& binding = m_Description.m_VertexBindings[current.m_uiVertexBufferSlot];

    Attribute attrib;
    attrib.m_uiLocation = pUsed->m_uiLocation;
    attrib.m_uiVertexBufferSlot = current.m_uiVertexBufferSlot;
    attrib.m_uiOffset = current.m_uiOffset;
    attrib.m_iComponents = components;
    attrib.m_eType = type;
    attrib.m_bNormalized = normalized;
    attrib.m_bInteger = integer;
    attrib.m_eRate = binding.m_Rate;
    attrib.m_uiDivisor = binding.m_Rate == ezGALVertexBindingRate::Vertex ? 0 : 1;
    m_Attributes.PushBack(attrib);
  }

  if (m_Attributes.IsEmpty())
  {
    ezLog::Error("Vertex declaration has no attributes that are used by the vertex shader.");
    return EZ_FAILURE;
  }

  return EZ_SUCCESS;
}

ezResult ezGALVertexDeclarationOpenGL::DeInitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);
  m_Attributes.Clear();
  m_VertexBufferStrides.Clear();
  return EZ_SUCCESS;
}
