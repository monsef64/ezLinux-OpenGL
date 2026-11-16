#include <RendererOpenGL/RendererOpenGLPCH.h>

EZ_STATICLINK_LIBRARY(RendererOpenGL)
{
  if (bReturn)
    return;

  EZ_STATICLINK_REFERENCE(RendererOpenGL_Device_Implementation_DeviceOpenGL);
}
