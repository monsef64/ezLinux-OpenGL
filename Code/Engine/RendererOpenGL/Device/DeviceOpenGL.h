#pragma once

#include <RendererOpenGL/RendererOpenGLDLL.h>

#include <Foundation/Types/Bitflags.h>
#include <Foundation/Types/UniquePtr.h>
#include <Foundation/Math/Size.h>
#include <RendererFoundation/Device/Device.h>
#include <RendererOpenGL/Utils/EGLLoader.h>

#include <glad/glad.h>

class ezGALCommandEncoder;
class ezGALCommandEncoderImplOpenGL;
class ezQueryPoolOpenGL;

/// OpenGL implementation of the graphics abstraction layer.
///
/// On Linux the backend uses EGL for context and surface management. A hidden 1x1 pbuffer surface backs the single
/// GL context that all rendering happens on; window swap-chains create an EGL window surface on top of the native
/// window and share the same context. All rendering to render targets happens through FBOs which are surface
/// independent; presents blit the back-buffer texture onto the default framebuffer of the window surface.
/// The device therefore keeps one GL context (current on the pbuffer surface) at all times, switches the
/// current surface only for the present blit, and switches back immediately.
class EZ_RENDEREROPENGL_DLL ezGALDeviceOpenGL : public ezGALDevice
{
private:
  friend ezInternal::NewInstance<ezGALDevice> CreateOpenGLDevice(ezAllocator* pAllocator, const ezGALDeviceCreationDescription& description);
  ezGALDeviceOpenGL(const ezGALDeviceCreationDescription& Description);

public:
  virtual ~ezGALDeviceOpenGL();

public:
  ezGALCommandEncoder* GetCommandEncoder() const;
  ezQueryPoolOpenGL& GetQueryPool() const;

  /// Registers a fence object created via glFenceSync so its lifetime is tracked.
  /// Fence handles handed out to the engine are the GLsync pointers cast to ezUInt64.
  void RegisterFence(GLsync pFence);

  /// Whether the GL context supports glClipControl (core 4.5 / ARB_clip_control) and it is enabled.
  bool HasClipControl() const { return m_bClipControl; }

  /// Accessors for the EGL setup shared with window swap-chains. All pointers are owned by the device.
  const ezEGL::FunctionTable* GetEGLFunctionTable() const { return m_pEGL; }
  void* GetEGLDisplay() const { return m_pEGLDisplay; }
  void* GetEGLContext() const { return m_pEGLContext; }
  void* GetPbufferSurface() const { return m_pPbufferSurface; }

  /// Makes the hidden pbuffer surface current again. Returns false if that fails.
  bool MakeDeviceContextCurrent() const;
  /// Makes the given EGL window surface current. Returns false if that fails.
  bool MakeWindowSurfaceCurrent(void* pWindowSurface) const;

protected:
  // Init & shutdown functions
  virtual ezStringView GetRendererPlatform() override;
  virtual ezResult InitPlatform() override;
  virtual ezResult ShutdownPlatform() override;

  // Command encoder functions
  virtual ezGALCommandEncoder* BeginCommandsPlatform(const char* szName) override;
  virtual void EndCommandsPlatform(ezGALCommandEncoder* pPass) override;

  virtual void FlushPlatform() override;
  virtual void WaitIdlePlatform() override;

  // State creation functions
  virtual ezGALBlendState* CreateBlendStatePlatform(const ezGALBlendStateCreationDescription& Description) override;
  virtual void DestroyBlendStatePlatform(ezGALBlendState* pBlendState) override;

  virtual ezGALDepthStencilState* CreateDepthStencilStatePlatform(const ezGALDepthStencilStateCreationDescription& Description) override;
  virtual void DestroyDepthStencilStatePlatform(ezGALDepthStencilState* pDepthStencilState) override;

  virtual ezGALRasterizerState* CreateRasterizerStatePlatform(const ezGALRasterizerStateCreationDescription& Description) override;
  virtual void DestroyRasterizerStatePlatform(ezGALRasterizerState* pRasterizerState) override;

  virtual ezGALSamplerState* CreateSamplerStatePlatform(const ezGALSamplerStateCreationDescription& Description) override;
  virtual void DestroySamplerStatePlatform(ezGALSamplerState* pSamplerState) override;
  virtual void RecreateSamplerStatePlatform(ezGALSamplerState* pSamplerState) override;

  virtual ezGALBindGroupLayout* CreateBindGroupLayoutPlatform(const ezGALBindGroupLayoutCreationDescription& Description) override;
  virtual void DestroyBindGroupLayoutPlatform(ezGALBindGroupLayout* pBindGroupLayout) override;

  virtual ezGALBindGroup* CreateBindGroupPlatform(const ezGALBindGroupCreationDescription& Description) override;
  virtual void DestroyBindGroupPlatform(ezGALBindGroup* pBindGroup) override;
  virtual void RecreateBindGroupPlatform(ezGALBindGroup* pBindGroup) override;

  virtual ezGALPipelineLayout* CreatePipelineLayoutPlatform(const ezGALPipelineLayoutCreationDescription& Description) override;
  virtual void DestroyPipelineLayoutPlatform(ezGALPipelineLayout* pPipelineLayout) override;

  virtual ezGALGraphicsPipeline* CreateGraphicsPipelinePlatform(const ezGALGraphicsPipelineCreationDescription& Description) override;
  virtual void DestroyGraphicsPipelinePlatform(ezGALGraphicsPipeline* pGraphicsPipeline) override;

  virtual ezGALComputePipeline* CreateComputePipelinePlatform(const ezGALComputePipelineCreationDescription& Description) override;
  virtual void DestroyComputePipelinePlatform(ezGALComputePipeline* pComputePipeline) override;

  // Resource creation functions
  virtual ezGALShader* CreateShaderPlatform(const ezGALShaderCreationDescription& Description) override;
  virtual void DestroyShaderPlatform(ezGALShader* pShader) override;

  virtual ezGALBuffer* CreateBufferPlatform(const ezGALBufferCreationDescription& Description, ezArrayPtr<const ezUInt8> pInitialData) override;
  virtual void DestroyBufferPlatform(ezGALBuffer* pBuffer) override;

  virtual ezGALTexture* CreateTexturePlatform(const ezGALTextureCreationDescription& Description, ezArrayPtr<ezGALSystemMemoryDescription> pInitialData) override;
  virtual void DestroyTexturePlatform(ezGALTexture* pTexture) override;

  virtual ezGALTexture* CreateSharedTexturePlatform(const ezGALTextureCreationDescription& Description, ezArrayPtr<ezGALSystemMemoryDescription> pInitialData, ezEnum<ezGALSharedTextureType> sharedType, ezGALPlatformSharedHandle handle) override;
  virtual void DestroySharedTexturePlatform(ezGALTexture* pTexture) override;

  virtual ezGALReadbackBuffer* CreateReadbackBufferPlatform(const ezGALBufferCreationDescription& Description) override;
  virtual void DestroyReadbackBufferPlatform(ezGALReadbackBuffer* pReadbackBuffer) override;

  virtual ezGALReadbackTexture* CreateReadbackTexturePlatform(const ezGALTextureCreationDescription& Description) override;
  virtual void DestroyReadbackTexturePlatform(ezGALReadbackTexture* pReadbackTexture) override;

  virtual ezGALRenderTargetView* CreateRenderTargetViewPlatform(ezGALTexture* pTexture, const ezGALRenderTargetViewCreationDescription& Description) override;
  virtual void DestroyRenderTargetViewPlatform(ezGALRenderTargetView* pRenderTargetView) override;

  // Other rendering creation functions
  virtual ezGALVertexDeclaration* CreateVertexDeclarationPlatform(const ezGALVertexDeclarationCreationDescription& Description) override;
  virtual void DestroyVertexDeclarationPlatform(ezGALVertexDeclaration* pVertexDeclaration) override;

  // Resource update functions
  virtual void UpdateBufferForNextFramePlatform(const ezGALBuffer* pBuffer, ezConstByteArrayPtr sourceData, ezUInt32 uiDestOffset) override;
  virtual void UpdateTextureForNextFramePlatform(const ezGALTexture* pTexture, const ezGALSystemMemoryDescription& sourceData, const ezGALTextureSubresource& destinationSubResource, const ezBoundingBoxu32& destinationBox) override;

  // GPU -> CPU query functions
  virtual ezEnum<ezGALAsyncResult> GetTimestampResultPlatform(ezGALTimestampHandle hTimestamp, ezTime& out_result) override;
  virtual ezEnum<ezGALAsyncResult> GetOcclusionResultPlatform(ezGALOcclusionHandle hOcclusion, ezUInt64& out_uiResult) override;
  virtual ezEnum<ezGALAsyncResult> GetFenceResultPlatform(ezGALFenceHandle hFence, ezTime timeout) override;
  virtual ezResult LockBufferPlatform(const ezGALReadbackBuffer* pBuffer, ezArrayPtr<const ezUInt8>& out_memory) const override;
  virtual void UnlockBufferPlatform(const ezGALReadbackBuffer* pBuffer) const override;
  virtual ezResult LockTexturePlatform(const ezGALReadbackTexture* pTexture, const ezArrayPtr<const ezGALTextureSubresource>& subResources, ezDynamicArray<ezGALSystemMemoryDescription>& out_memory) const override;
  virtual void UnlockTexturePlatform(const ezGALReadbackTexture* pTexture, const ezArrayPtr<const ezGALTextureSubresource>& subResources) const override;

  // Misc functions
  virtual void BeginFramePlatform(ezArrayPtr<ezGALSwapChain*> swapchains, const ezUInt64 uiAppFrame) override;
  virtual void EndFramePlatform(ezArrayPtr<ezGALSwapChain*> swapchains) override;

  virtual ezUInt64 GetCurrentFramePlatform() const override;
  virtual ezUInt64 GetSafeFramePlatform() const override;

  virtual void FillCapabilitiesPlatform() override;

  virtual const ezGALSharedTexture* GetSharedTexture(ezGALTextureHandle hTexture) const override;

private:
  ezResult InitEGL();
  void DeinitEGL();

  /// Creates a GLsync fence, registers it and returns the handle.
  ezGALFenceHandle SubmitFence();
  /// Poll all tracked fences and release the ones that have signaled. Advances m_uiSafeFrame.
  void ReclaimFinishedFrames();

  static constexpr ezUInt32 FRAMES = 4;

  const ezEGL::FunctionTable* m_pEGL = nullptr;
  void* m_pEGLDisplay = nullptr;
  void* m_pEGLContext = nullptr;
  void* m_pPbufferSurface = nullptr;
  ezEGL::EGLint m_iEGLMajor = 0;
  ezEGL::EGLint m_iEGLMinor = 0;

  bool m_bClipControl = false;
  bool m_bEGLFailed = false;
  /// Set to true once the GL entry points were loaded (after a GL context was made current). Guards all raw GL calls
  /// so the device can be shut down safely even when Init failed partway through (e.g. no EGL available).
  bool m_bGLInitialized = false;

  ezUniquePtr<ezGALCommandEncoderImplOpenGL> m_pCommandEncoderImpl;
  ezUniquePtr<ezGALCommandEncoder> m_pCommandEncoder;
  ezUniquePtr<ezQueryPoolOpenGL> m_pQueryPool;

  struct PerFrameData
  {
    ezGALFenceHandle m_hFence = {};
    ezUInt64 m_uiFrame = ezUInt64(-1);
  };

  PerFrameData m_PerFrameData[FRAMES];

  /// All GLsync objects created by this device (user fences and per-frame fences) so they can be released at shutdown.
  ezDynamicArray<GLsync> m_Fences;

  ezUInt64 m_uiFrameCounter = 1;
  ezUInt64 m_uiSafeFrame = 0;
  ezUInt8 m_uiCurrentPerFrameData = m_uiFrameCounter % FRAMES;
};
