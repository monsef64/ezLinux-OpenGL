#pragma once

#include <RendererOpenGL/RendererOpenGLDLL.h>

#include <Foundation/Containers/HashTable.h>
#include <Foundation/Containers/HybridArray.h>
#include <Foundation/Containers/StaticArray.h>
#include <RendererFoundation/CommandEncoder/CommandEncoderPlatformInterface.h>
#include <RendererFoundation/Resources/RenderTargetSetup.h>

#include <glad/glad.h>

class ezGALDeviceOpenGL;
class ezGALBufferOpenGL;
class ezGALTextureOpenGL;
class ezGALSamplerStateOpenGL;
class ezGALVertexDeclarationOpenGL;
class ezGALGraphicsPipelineOpenGL;
class ezGALComputePipelineOpenGL;
class ezGALShaderOpenGL;
class ezGALRenderTargetViewOpenGL;

/// OpenGL command encoder. GL is single-threaded immediate mode, so most GL commands are emitted directly.
/// The encoder keeps bound resource state and defers the (program dependent) flat binding to draw/dispatch time.
class EZ_RENDEREROPENGL_DLL ezGALCommandEncoderImplOpenGL : public ezGALCommandEncoderCommonPlatformInterface
{
public:
  ezGALCommandEncoderImplOpenGL(ezGALDeviceOpenGL& ref_device);
  ~ezGALCommandEncoderImplOpenGL();

  // ezGALCommandEncoderCommonPlatformInterface

  virtual void SetBindGroupPlatform(ezUInt32 uiBindGroup, const ezGALBindGroupCreationDescription& bindGroup) override;
  virtual void SetBindGroupPlatform(ezUInt32 uiBindGroup, const ezGALBindGroup* pBindGroup) override;
  virtual void SetPushConstantsPlatform(ezArrayPtr<const ezUInt8> data) override;

  // GPU -> CPU query functions

  virtual ezGALTimestampHandle InsertTimestampPlatform() override;
  virtual ezGALOcclusionHandle BeginOcclusionQueryPlatform(ezEnum<ezGALQueryType> type) override;
  virtual void EndOcclusionQueryPlatform(ezGALOcclusionHandle hOcclusion) override;
  virtual ezGALFenceHandle InsertFencePlatform() override;

  // Resource update functions

  virtual void CopyBufferPlatform(const ezGALBuffer* pDestination, const ezGALBuffer* pSource) override;
  virtual void CopyBufferRegionPlatform(const ezGALBuffer* pDestination, ezUInt32 uiDestOffset, const ezGALBuffer* pSource, ezUInt32 uiSourceOffset, ezUInt32 uiByteCount) override;

  virtual void UpdateBufferPlatform(const ezGALBuffer* pDestination, ezUInt32 uiDestOffset, ezArrayPtr<const ezUInt8> sourceData, ezGALUpdateMode::Enum updateMode) override;

  virtual void CopyTexturePlatform(const ezGALTexture* pDestination, const ezGALTexture* pSource) override;
  virtual void CopyTextureRegionPlatform(const ezGALTexture* pDestination, const ezGALTextureSubresource& destinationSubResource, const ezVec3U32& vDestinationPoint, const ezGALTexture* pSource, const ezGALTextureSubresource& sourceSubResource, const ezBoundingBoxu32& box) override;

  virtual void UpdateTexturePlatform(const ezGALTexture* pDestination, const ezGALTextureSubresource& destinationSubResource, const ezBoundingBoxu32& destinationBox, const ezGALSystemMemoryDescription& sourceData) override;

  virtual void ResolveTexturePlatform(const ezGALTexture* pDestination, const ezGALTextureSubresource& destinationSubResource, const ezGALTexture* pSource, const ezGALTextureSubresource& sourceSubResource) override;

  virtual void ReadbackTexturePlatform(const ezGALReadbackTexture* pDestination, const ezGALTexture* pSource) override;
  virtual void ReadbackBufferPlatform(const ezGALReadbackBuffer* pDestination, const ezGALBuffer* pSource) override;

  // Barriers

  virtual void TextureBarrierPlatform(ezArrayPtr<const ezGALTextureBarrier> barriers) override;
  virtual void BufferBarrierPlatform(ezArrayPtr<const ezGALBufferBarrier> barriers) override;

  // Misc

  virtual void FlushPlatform() override;

  // Debug helper functions

  virtual void PushMarkerPlatform(const char* szMarker) override;
  virtual void PopMarkerPlatform() override;
  virtual void InsertEventMarkerPlatform(const char* szMarker) override;

  // Compute Dispatch

  virtual void BeginComputePlatform() override;
  virtual void EndComputePlatform() override;

  virtual ezResult DispatchPlatform(ezUInt32 uiThreadGroupCountX, ezUInt32 uiThreadGroupCountY, ezUInt32 uiThreadGroupCountZ) override;
  virtual ezResult DispatchIndirectPlatform(const ezGALBuffer* pIndirectArgumentBuffer, ezUInt32 uiArgumentOffsetInBytes) override;

  // Draw functions

  virtual void BeginRenderingPlatform(const ezGALRenderingSetup& renderingSetup) override;
  virtual void EndRenderingPlatform() override;

  virtual void ClearPlatform(const ezColor& clearColor, ezUInt32 uiRenderTargetClearMask, bool bClearDepth, bool bClearStencil, float fDepthClear, ezUInt8 uiStencilClear) override;

  virtual ezResult DrawPlatform(ezUInt32 uiVertexCount, ezUInt32 uiStartVertex) override;
  virtual ezResult DrawIndexedPlatform(ezUInt32 uiIndexCount, ezUInt32 uiStartIndex) override;
  virtual ezResult DrawIndexedInstancedPlatform(ezUInt32 uiIndexCountPerInstance, ezUInt32 uiInstanceCount, ezUInt32 uiStartIndex) override;
  virtual ezResult DrawIndexedInstancedIndirectPlatform(const ezGALBuffer* pIndirectArgumentBuffer, ezUInt32 uiArgumentOffsetInBytes) override;
  virtual ezResult DrawInstancedPlatform(ezUInt32 uiVertexCountPerInstance, ezUInt32 uiInstanceCount, ezUInt32 uiStartVertex) override;
  virtual ezResult DrawInstancedIndirectPlatform(const ezGALBuffer* pIndirectArgumentBuffer, ezUInt32 uiArgumentOffsetInBytes) override;

  // State functions

  virtual void SetIndexBufferPlatform(const ezGALBuffer* pIndexBuffer) override;
  virtual void SetVertexBufferPlatform(ezUInt32 uiSlot, const ezGALBuffer* pVertexBuffer, ezUInt32 uiOffset) override;

  virtual void SetGraphicsPipelinePlatform(const ezGALGraphicsPipeline* pGraphicsPipeline) override;
  virtual void SetComputePipelinePlatform(const ezGALComputePipeline* pComputePipeline) override;

  // Dynamic State Functions

  virtual void SetViewportPlatform(const ezRectFloat& rect, float fMinDepth, float fMaxDepth) override;
  virtual void SetScissorRectPlatform(const ezRectU32& rect) override;
  virtual void SetStencilReferencePlatform(ezUInt8 uiStencilRefValue) override;

private:
  /// Bound resources, flat binding is applied against the current program at draw time.
  /// NOTE: the encoder lives across frames while GAL objects get destroyed at the end of frames,
  /// so bound resources are cached as handles/GL names/value copies, never as raw pointers, which would dangle.
  struct BoundBufferRange
  {
    ezGALBufferHandle m_hBuffer;
    ezUInt32 m_uiOffset = 0;
    ezUInt32 m_uiSize = EZ_GAL_WHOLE_SIZE;
  };

  struct BoundTexture
  {
    ezGALTextureHandle m_hTexture;
    ezGALTextureRange m_Range;
  };

  struct BoundImage
  {
    ezGALTextureHandle m_hTexture;
    ezUInt32 m_uiLevel = 0;
    ezUInt32 m_uiLayer = 0;
    bool m_bLayered = false;
  };

  static ezUInt32 MakeBindKey(ezUInt32 uiSet, ezUInt32 uiSlot) { return (uiSet << 16) | (uiSlot & 0xFFFFu); }

  /// Returns the underlying platform texture for a (potentially proxy) texture.
  static const ezGALTextureOpenGL* ResolvePlatformTexture(const ezGALTexture* pTexture);

  void ApplyBindGroup(ezUInt32 uiBindGroup, const ezGALBindGroupCreationDescription& bindGroup);
  void FlushDeferredResourceState();
  void FlushDeferredPipelineState();
  void ApplyBlendState(const struct ezGALBlendStateCreationDescription& desc, ezUInt32 uiColorTargetCount);
  void ApplyDepthStencilState(const struct ezGALDepthStencilStateCreationDescription& desc);
  void ApplyRasterizerState(const struct ezGALRasterizerStateCreationDescription& desc);
  void ApplyVertexState();
  void ApplyViewportAndScissor();
  void ApplyStencilReference();
  void FlushBeforeRasterDraw();
  void SetDrawBuffersMask(ezUInt32 uiMask);
  GLenum GetPrimitiveMode() const;

  GLuint GetOrCreateFBO(const ezGALFrameBufferDescriptor& desc, ezUInt32 uiColorTargetCount);
  void AttachTexture(GLuint uiFBO, GLenum eAttachment, const ezGALRenderTargetView* pView);
  ezUInt32 BindFBO(const ezGALFrameBufferDescriptor& desc, ezUInt32 uiColorTargetCount);

  void InitializePushConstantBuffer();
  void InitializeVAO();

  ezGALDeviceOpenGL& m_DeviceOpenGL;

  // Push constant storage. The SPIR-V push constant block is emitted as a UBO with a dedicated flat binding.
  GLuint m_uiPushConstantBuffer = 0;

  // Single VAO with vertex state re-applied whenever it is dirty.
  GLuint m_uiVAO = 0;
  bool m_bVertexStateDirty = true;
  ezHybridArray<ezUInt32, EZ_GAL_MAX_VERTEX_ATTRIBUTE_COUNT> m_EnabledAttributeLocations;
  ezStaticArray<GLuint, EZ_GAL_MAX_VERTEX_BUFFER_COUNT> m_BoundVertexBuffers;
  ezStaticArray<ezUInt32, EZ_GAL_MAX_VERTEX_BUFFER_COUNT> m_VertexBufferOffsets;

  bool m_bIndexBufferBound = false;
  ezEnum<ezGALIndexType> m_BoundIndexType = ezGALIndexType::UInt;

  // Bound resources, flat binding is applied against the current program at draw time. See note above.
  ezHashTable<ezUInt32, ezGALBufferHandle> m_BoundUniformBuffers;
  ezHashTable<ezUInt32, BoundBufferRange> m_BoundStorageBuffers;
  ezHashTable<ezUInt32, BoundTexture> m_BoundTextures;
  ezHashTable<ezUInt32, BoundImage> m_BoundImages;
  ezHashTable<ezUInt32, ezGALSamplerStateHandle> m_BoundSamplers;
  bool m_bResourceStateDirty = true;

  // Bound pipelines (cached by value; the pipeline objects themselves may be destroyed while the encoder persists)
  ezGALGraphicsPipelineCreationDescription m_BoundGraphicsPipelineDesc;
  ezGALComputePipelineCreationDescription m_BoundComputePipelineDesc;
  bool m_bGraphicsPipelineSet = false;
  bool m_bComputePipelineSet = false;
  bool m_bPipelineStateDirty = true;
  bool m_bStencilRefDirty = true;
  ezUInt8 m_uiStencilRef = 0;

  // Viewport / scissor
  ezRectFloat m_Viewport{0.0f, 0.0f, 1.0f, 1.0f};
  float m_fMinDepth = 0.0f;
  float m_fMaxDepth = 1.0f;
  ezRectU32 m_ScissorRect{0, 0, 0, 0};
  bool m_bScissorRectValid = false;
  bool m_bScissorEnabled = false; ///< Set by the rasterizer state; AND-ed with m_bScissorRectValid.

  // Current render target state
  bool m_bInRendering = false;
  ezGALRenderingSetup m_RenderTargetSetup;
  ezSizeU32 m_RenderTargetSize{0, 0};
  ezUInt32 m_uiColorTargetCount = 0;
  bool m_bDepthReadOnly = false;
  bool m_bMultisampled = false;
  ezGALFrameBufferDescriptor m_FrameBuffer;

  // Transient FBOs used for resolves and copies
  GLuint m_uiResolveFBO[2] = {0, 0};
  GLuint m_uiCopyFBO[2] = {0, 0};

  // Cached FBOs for rendering, keyed by the frame buffer description (view handles + size)
  struct FBOEntry
  {
    ezGALFrameBufferDescriptor m_Desc;
    GLuint m_uiFBO = 0;
    ezUInt32 m_uiDrawBuffersMask = 0;
  };
  ezDynamicArray<FBOEntry> m_FBOCache;
  GLuint m_uiCurrentFBO = 0;
  ezUInt32 m_uiCurrentDrawBuffersMask = 0;
};
