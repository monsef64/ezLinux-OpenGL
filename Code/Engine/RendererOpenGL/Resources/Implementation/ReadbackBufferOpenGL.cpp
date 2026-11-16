#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <RendererOpenGL/Resources/ReadbackBufferOpenGL.h>

ezGALReadbackBufferOpenGL::ezGALReadbackBufferOpenGL(const ezGALBufferCreationDescription& Description)
  : ezGALReadbackBuffer(Description)
{
}

ezGALReadbackBufferOpenGL::~ezGALReadbackBufferOpenGL() = default;

ezResult ezGALReadbackBufferOpenGL::InitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);

  const ezUInt32 uiSize = m_Description.m_uiTotalSize;
  if (uiSize == 0)
  {
    ezLog::Error("Readback buffer size is 0");
    return EZ_FAILURE;
  }

  glCreateBuffers(1, &m_uiBuffer);
  if (m_uiBuffer == 0)
  {
    ezLog::Error("Failed to create readback buffer");
    return EZ_FAILURE;
  }

  // GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT so the buffer can be read without a copy while the GPU fence will tell us when it is ready.
  glNamedBufferStorage(m_uiBuffer, static_cast<GLsizeiptr>(uiSize), nullptr, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT);
  return EZ_SUCCESS;
}

ezResult ezGALReadbackBufferOpenGL::DeInitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);

  if (m_uiBuffer != 0)
  {
    glDeleteBuffers(1, &m_uiBuffer);
    m_uiBuffer = 0;
  }
  return EZ_SUCCESS;
}
