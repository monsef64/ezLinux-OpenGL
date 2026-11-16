if (TARGET MeshRenderSample AND TARGET RendererDX11)
  add_dependencies(MeshRenderSample RendererDX11)
endif()

if (TARGET MeshRenderSample AND TARGET RendererVulkan)
  add_dependencies(MeshRenderSample RendererVulkan)
endif()

if (TARGET MeshRenderSample AND TARGET RendererOpenGL)
  add_dependencies(MeshRenderSample RendererOpenGL)
endif()
