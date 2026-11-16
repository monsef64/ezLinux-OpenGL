#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <RendererOpenGL/Resources/BufferOpenGL.h>

ezGALBufferOpenGL::ezGALBufferOpenGL(const ezGALBufferCreationDescription& Description)
  : ezGALBuffer(Description)
{
}

ezGALBufferOpenGL::~ezGALBufferOpenGL() = default;

ezResult ezGALBufferOpenGL::InitPlatform(ezGALDevice* pDevice, ezArrayPtr<const ezUInt8> pInitialData)
{
  EZ_IGNORE_UNUSED(pDevice);

  const ezUInt32 uiSize = GetSize();
  if (uiSize == 0)
  {
    ezLog::Error("Cannot create a buffer of size 0");
    return EZ_FAILURE;
  }

  m_IndexType = m_Description.m_uiStructSize == 2 ? ezGALIndexType::UShort : ezGALIndexType::UInt;

  const bool bDynamic = m_Description.m_BufferFlags.IsSet(ezGALBufferUsageFlags::Transient) || !m_Description.m_ResourceAccess.IsImmutable();
  const GLenum usage = bDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;

  if (glCreateBuffers != nullptr)
  {
    glCreateBuffers(1, &m_uiBuffer);
    glNamedBufferData(m_uiBuffer, static_cast<GLsizeiptr>(uiSize), pInitialData.GetPtr(), usage);
  }
  else
  {
    glGenBuffers(1, &m_uiBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_uiBuffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(uiSize), pInitialData.GetPtr(), usage);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  if (m_uiBuffer == 0)
  {
    ezLog::Error("Failed to create GL buffer");
    return EZ_FAILURE;
  }

  return EZ_SUCCESS;
}

ezResult ezGALBufferOpenGL::DeInitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);

  if (m_uiBuffer != 0)
  {
    if (glDeleteBuffers != nullptr)
    {
      glDeleteBuffers(1, &m_uiBuffer);
    }
    else
    {
      const GLuint uiBuffer = m_uiBuffer;
      glBindBuffer(GL_ARRAY_BUFFER, uiBuffer);
      glDeleteBuffers(1, &uiBuffer);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    m_uiBuffer = 0;
  }
  return EZ_SUCCESS;
}
