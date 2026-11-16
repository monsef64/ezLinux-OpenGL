#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <Core/ResourceManager/Resource.h>
#include <Core/System/Window.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/Profiling/Profiling.h>
#include <Foundation/Utilities/Stats.h>
#include <RendererFoundation/CommandEncoder/CommandEncoder.h>
#include <RendererFoundation/Device/DeviceFactory.h>
#include <RendererFoundation/Device/SwapChain.h>
#include <RendererFoundation/Profiling/Profiling.h>
#include <RendererFoundation/RendererReflection.h>
#include <RendererFoundation/Resources/ResourceFormats.h>
#include <RendererOpenGL/CommandEncoder/CommandEncoderImplOpenGL.h>
#include <RendererOpenGL/Device/DeviceOpenGL.h>
#include <RendererOpenGL/Device/SwapChainOpenGL.h>
#include <RendererOpenGL/Pools/QueryPoolOpenGL.h>
#include <RendererOpenGL/Resources/BufferOpenGL.h>
#include <RendererOpenGL/Resources/ReadbackBufferOpenGL.h>
#include <RendererOpenGL/Resources/ReadbackTextureOpenGL.h>
#include <RendererOpenGL/Resources/RenderTargetViewOpenGL.h>
#include <RendererOpenGL/Resources/SharedTextureOpenGL.h>
#include <RendererOpenGL/Resources/TextureOpenGL.h>
#include <RendererOpenGL/Shader/BindGroupLayoutOpenGL.h>
#include <RendererOpenGL/Shader/BindGroupOpenGL.h>
#include <RendererOpenGL/Shader/PipelineLayoutOpenGL.h>
#include <RendererOpenGL/Shader/ShaderOpenGL.h>
#include <RendererOpenGL/Shader/VertexDeclarationOpenGL.h>
#include <RendererOpenGL/State/PipelinesOpenGL.h>
#include <RendererOpenGL/State/StateOpenGL.h>
#include <RendererOpenGL/Utils/EGLLoader.h>
#include <RendererOpenGL/Utils/MappingTablesOpenGL.h>

#include <Foundation/Math/Declarations.h>

#include <glad/glad.h>

#include <cstdio>
#include <cstdlib>

ezInternal::NewInstance<ezGALDevice> CreateOpenGLDevice(ezAllocator* pAllocator, const ezGALDeviceCreationDescription& description)
{
  return EZ_NEW(pAllocator, ezGALDeviceOpenGL, description);
}

// clang-format off
EZ_BEGIN_SUBSYSTEM_DECLARATION(RendererOpenGL, DeviceFactoryOpenGL)

ON_CORESYSTEMS_STARTUP
{
  // Force graphics resource updates onto the main thread.
  // ezEngine otherwise updates shaders, textures and meshes on worker threads. The OpenGL backend uses a single GL
  // context that is current only on the main thread, so GAL resource creation on worker threads is not possible without
  // per-thread shared EGL contexts. ezResource::UpdateGraphicsResource is the renderer-configured knob for this.
  ezResource::UpdateGraphicsResource = ezResource::DoUpdate::OnMainThread;

  // The OpenGL backend consumes SPIR-V bytecode that is converted to GLSL at load time.
  // It therefore registers its own shader model ("OPENGL") together with the ezShaderCompilerOpenGL
  // plugin, which inherits the DXC based SPIR-V compiler from ezShaderCompilerVulkan. The shaders
  // are compiled with the OPENGL define (see Platforms.h / Platform_OpenGL.h) and the produced
  // SPIR-V modules are cross-compiled to GLSL at load time by the renderer itself.
  ezGALDeviceFactory::RegisterCreatorFunc("OpenGL", &CreateOpenGLDevice, "OPENGL", "ezShaderCompilerOpenGL");
}

ON_CORESYSTEMS_SHUTDOWN
{
  ezGALDeviceFactory::UnregisterCreatorFunc("OpenGL");
}

EZ_END_SUBSYSTEM_DECLARATION;
// clang-format on

ezGALDeviceOpenGL::ezGALDeviceOpenGL(const ezGALDeviceCreationDescription& Description)
  : ezGALDevice(Description)
{
  // Same configuration as ON_CORESYSTEMS_STARTUP above; repeated here so it also applies
  // when the backend is only linked and the subsystem has not been started yet.
  // The single GL context is only current on the main thread.
  ezResource::UpdateGraphicsResource = ezResource::DoUpdate::OnMainThread;
}

ezGALDeviceOpenGL::~ezGALDeviceOpenGL() = default;

// #####################################
// ## Init & Shutdown
// #####################################

ezStringView ezGALDeviceOpenGL::GetRendererPlatform()
{
  return "OPENGL";
}

ezResult ezGALDeviceOpenGL::InitPlatform()
{
  EZ_LOG_BLOCK("ezGALDeviceOpenGL::InitPlatform");

  if (m_Description.m_bDebugDevice)
  {
    ezLog::Info("OpenGL: debug device requested");
  }

  EZ_SUCCEED_OR_RETURN(InitEGL());

  if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(m_pEGL->eglGetProcAddress)) == 0)
  {
    ezLog::Error("Failed to load OpenGL function pointers via eglGetProcAddress");
    DeinitEGL();
    return EZ_FAILURE;
  }
  m_bGLInitialized = true;

  const GLubyte* szVersion = glGetString(GL_VERSION);
  ezLog::Info("OpenGL: {0}", szVersion != nullptr ? reinterpret_cast<const char*>(szVersion) : "<unknown>");

  int iMajor = 0, iMinor = 0;
  if (szVersion != nullptr)
  {
    // GL_VERSION is either "4.6 (Core Profile) Mesa 23.x" or "OpenGL ES 3.x ..."
    const char* s = reinterpret_cast<const char*>(szVersion);
    const char* p = s;
    while (*p != '\0' && (*p < '0' || *p > '9'))
      ++p;
    sscanf(p, "%d.%d", &iMajor, &iMinor);
  }

  // glClipControl is core since GL 4.5; without it we cannot render with D3D/Vulkan style clip-space conventions
  // and have to fall back to GL's natural lower-left origin.
  m_bClipControl = (iMajor > 4) || (iMajor == 4 && iMinor >= 5);
  if (m_bClipControl)
  {
    glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
  }
  else
  {
    ezLog::Warning("OpenGL context reports version {0}.{1}; GL >= 4.5 is required for glClipControl. Rendering will use the legacy GL clip-space convention and might appear flipped vertically.", iMajor, iMinor);
  }

  ezClipSpaceDepthRange::Default = ezClipSpaceDepthRange::ZeroToOne;
  ezClipSpaceYMode::RenderToTextureDefault = m_bClipControl ? ezClipSpaceYMode::Regular : ezClipSpaceYMode::Flipped;

  {
    GLint iMaxTextureImageUnits = 0;
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &iMaxTextureImageUnits);
    ezLog::Info("OpenGL: max combined texture image units: {0}", iMaxTextureImageUnits);
  }

  m_pQueryPool = EZ_DEFAULT_NEW(ezQueryPoolOpenGL, this);
  if (m_pQueryPool->Initialize().Failed())
  {
    ezLog::Error("Failed to initialize the OpenGL query pool");
    m_pQueryPool.Clear();
    DeinitEGL();
    return EZ_FAILURE;
  }

  m_pCommandEncoderImpl = EZ_DEFAULT_NEW(ezGALCommandEncoderImplOpenGL, *this);
  m_pCommandEncoder = EZ_DEFAULT_NEW(ezGALCommandEncoder, *this, *m_pCommandEncoderImpl);

  ezGALWindowSwapChain::SetFactoryMethod([this](const ezGALWindowSwapChainCreationDescription& desc) -> ezGALSwapChainHandle
    { return CreateSwapChain([&desc](ezAllocator* pAllocator) -> ezGALSwapChain*
        { return EZ_NEW(pAllocator, ezGALSwapChainWindowOpenGL, desc); }); });

  return EZ_SUCCESS;
}

ezResult ezGALDeviceOpenGL::ShutdownPlatform()
{
  // Make sure the GPU is done with all work before we tear down GL objects.
  // GL entry points may never have been loaded if Init failed partway through.
  if (m_bGLInitialized)
  {
    MakeDeviceContextCurrent();
    glFinish();
  }

  ezGALWindowSwapChain::SetFactoryMethod({});

  DestroyDeadObjects();

  m_pCommandEncoderImpl.Clear();
  m_pCommandEncoder.Clear();

  if (m_bGLInitialized)
  {
    for (GLsync& fence : m_Fences)
    {
      if (fence != nullptr)
      {
        glDeleteSync(fence);
      }
    }
  }
  m_Fences.Clear();
  for (PerFrameData& data : m_PerFrameData)
  {
    data.m_hFence = {};
    data.m_uiFrame = ezUInt64(-1);
  }

  if (m_pQueryPool)
  {
    m_pQueryPool->DeInitialize();
    m_pQueryPool.Clear();
  }

  DeinitEGL();
  return EZ_SUCCESS;
}

// #####################################
// ## EGL management
// #####################################

ezResult ezGALDeviceOpenGL::InitEGL()
{
  ezStringBuilder sError;
  m_pEGL = ezEGL::LoadFunctionTable(sError);
  if (m_pEGL == nullptr)
  {
    ezLog::Error("Failed to load EGL: {0}", sError);
    return EZ_FAILURE;
  }

  void* pDisplay = m_pEGL->eglGetDisplay(ezEGL::EGL_DEFAULT_DISPLAY_HANDLE);
  if (pDisplay == nullptr)
  {
    ezLog::Error("eglGetDisplay(EGL_DEFAULT_DISPLAY) failed: {0}", ezStringView(ezEGL::GetErrorString(m_pEGL->eglGetError())));
    return EZ_FAILURE;
  }
  m_pEGLDisplay = pDisplay;

  if (m_pEGL->eglInitialize(m_pEGLDisplay, &m_iEGLMajor, &m_iEGLMinor) == 0)
  {
    ezLog::Error("eglInitialize failed: {0}", ezStringView(ezEGL::GetErrorString(m_pEGL->eglGetError())));
    return EZ_FAILURE;
  }

  const char* szVendor = m_pEGL->eglQueryString(m_pEGLDisplay, 0x3053 /* EGL_VENDOR */);
  const char* szClientAPIs = m_pEGL->eglQueryString(m_pEGLDisplay, 0x308D /* EGL_CLIENT_APIS */);
  ezLog::Info("OpenGL: EGL {0}.{1}, vendor '{2}', client APIs '{3}'", m_iEGLMajor, m_iEGLMinor, szVendor != nullptr ? szVendor : "<unknown>", szClientAPIs != nullptr ? szClientAPIs : "<unknown>");

  // The initial client API is EGL_OPENGL_ES_API. Desktop OpenGL context creation requires
  // binding the OpenGL client API first, otherwise eglCreateContext fails with EGL_BAD_MATCH
  // (e.g. on Mesa).
  if (m_pEGL->eglBindAPI == nullptr || m_pEGL->eglBindAPI(ezEGL::EGL_OPENGL_API) == 0)
  {
    ezLog::Error("eglBindAPI(EGL_OPENGL_API) failed: {0}", m_pEGL->eglBindAPI == nullptr ? ezStringView("symbol not found") : ezStringView(ezEGL::GetErrorString(m_pEGL->eglGetError())));
    DeinitEGL();
    return EZ_FAILURE;
  }

  // Choose a config that is usable both for the hidden pbuffer and for window surfaces.
  // Headless EGL platforms (e.g. Mesa's surfaceless platform) provide no WINDOW-capable
  // configs, so fall back to a PBUFFER-only config in that case. Window swap-chains pick their
  // own window-capable configs independently when a native window exists.
  const ezEGL::EGLint configAttribs[] = {
    ezEGL::EGL_RED_SIZE,
    8,
    ezEGL::EGL_GREEN_SIZE,
    8,
    ezEGL::EGL_BLUE_SIZE,
    8,
    ezEGL::EGL_ALPHA_SIZE,
    8,
    ezEGL::EGL_DEPTH_SIZE,
    24,
    ezEGL::EGL_STENCIL_SIZE,
    8,
    ezEGL::EGL_SURFACE_TYPE,
    ezEGL::EGL_WINDOW_BIT | ezEGL::EGL_PBUFFER_BIT,
    ezEGL::EGL_RENDERABLE_TYPE,
    ezEGL::EGL_OPENGL_BIT,
    ezEGL::EGL_NONE,
  };
  const ezEGL::EGLint configAttribsPbufferOnly[] = {
    ezEGL::EGL_RED_SIZE,
    8,
    ezEGL::EGL_GREEN_SIZE,
    8,
    ezEGL::EGL_BLUE_SIZE,
    8,
    ezEGL::EGL_ALPHA_SIZE,
    8,
    ezEGL::EGL_DEPTH_SIZE,
    24,
    ezEGL::EGL_STENCIL_SIZE,
    8,
    ezEGL::EGL_SURFACE_TYPE,
    ezEGL::EGL_PBUFFER_BIT,
    ezEGL::EGL_RENDERABLE_TYPE,
    ezEGL::EGL_OPENGL_BIT,
    ezEGL::EGL_NONE,
  };

  ezEGL::EGLConfig config = nullptr;
  ezEGL::EGLint iConfigCount = 0;
  const ezEGL::EGLBoolean bChooseOk = m_pEGL->eglChooseConfig(m_pEGLDisplay, configAttribs, &config, 1, &iConfigCount);
  if ((bChooseOk == 0 || iConfigCount == 0))
  {
    ezEGL::EGLConfig pbufferConfig = nullptr;
    ezEGL::EGLint iPbufferConfigCount = 0;
    const ezEGL::EGLBoolean bPbufferOk = m_pEGL->eglChooseConfig(m_pEGLDisplay, configAttribsPbufferOnly, &pbufferConfig, 1, &iPbufferConfigCount);
    if (bPbufferOk != 0 && iPbufferConfigCount > 0)
    {
      ezLog::Info("OpenGL: no window-capable EGL config found; falling back to a pbuffer-only config (headless mode)");
      config = pbufferConfig;
      iConfigCount = iPbufferConfigCount;
    }
    else if (bChooseOk == 0)
    {
      ezLog::Error("eglChooseConfig failed: {0}", ezStringView(ezEGL::GetErrorString(m_pEGL->eglGetError())));
      DeinitEGL();
      return EZ_FAILURE;
    }
    else
    {
      ezLog::Error("eglChooseConfig failed (no suitable config found)");
      DeinitEGL();
      return EZ_FAILURE;
    }
  }

  // Try to create a core profile context; fall back to a lower version if the driver refuses.
  const ezEGL::EGLint contextAttribs45[] = {
    ezEGL::EGL_CONTEXT_MAJOR_VERSION_KHR,
    4,
    ezEGL::EGL_CONTEXT_MINOR_VERSION_KHR,
    5,
    ezEGL::EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
    ezEGL::EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
    ezEGL::EGL_CONTEXT_FLAGS_KHR,
    m_Description.m_bDebugDevice ? ezEGL::EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR : 0,
    ezEGL::EGL_NONE,
  };
  const ezEGL::EGLint contextAttribs33[] = {
    ezEGL::EGL_CONTEXT_MAJOR_VERSION_KHR,
    3,
    ezEGL::EGL_CONTEXT_MINOR_VERSION_KHR,
    3,
    ezEGL::EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
    ezEGL::EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
    ezEGL::EGL_NONE,
  };

  void* pContext = m_pEGL->eglCreateContext(m_pEGLDisplay, config, nullptr, contextAttribs45);
  if (pContext == nullptr)
  {
    ezLog::Warning("eglCreateContext for OpenGL 4.5 core failed ({0}), trying 3.3 core", ezStringView(ezEGL::GetErrorString(m_pEGL->eglGetError())));
    pContext = m_pEGL->eglCreateContext(m_pEGLDisplay, config, nullptr, contextAttribs33);
  }
  if (pContext == nullptr)
  {
    ezLog::Error("eglCreateContext failed: {0}", ezStringView(ezEGL::GetErrorString(m_pEGL->eglGetError())));
    DeinitEGL();
    return EZ_FAILURE;
  }
  m_pEGLContext = pContext;
  // The config that was finally used has to be reused for the hidden pbuffer surface
  // (eglGetConfigAttrib is not tracked, so the one from eglChooseConfig is used).
  ezEGL::EGLConfig usedConfig = config;

  const ezEGL::EGLint pbufferAttribs[] = {
    ezEGL::EGL_WIDTH,
    1,
    ezEGL::EGL_HEIGHT,
    1,
    ezEGL::EGL_NONE,
  };
  m_pPbufferSurface = m_pEGL->eglCreatePbufferSurface(m_pEGLDisplay, usedConfig, pbufferAttribs);
  if (m_pPbufferSurface == nullptr)
  {
    ezLog::Error("eglCreatePbufferSurface failed: {0}", ezStringView(ezEGL::GetErrorString(m_pEGL->eglGetError())));
    DeinitEGL();
    return EZ_FAILURE;
  }

  if (!MakeDeviceContextCurrent())
  {
    ezLog::Error("Failed to make the EGL context current");
    DeinitEGL();
    return EZ_FAILURE;
  }

  return EZ_SUCCESS;
}

void ezGALDeviceOpenGL::DeinitEGL()
{
  if (m_pEGL == nullptr)
    return;

  if (m_pEGLDisplay != nullptr)
  {
    m_pEGL->eglMakeCurrent(m_pEGLDisplay, nullptr, nullptr, nullptr);
  }
  if (m_pPbufferSurface != nullptr)
  {
    m_pEGL->eglDestroySurface(m_pEGLDisplay, m_pPbufferSurface);
    m_pPbufferSurface = nullptr;
  }
  if (m_pEGLContext != nullptr)
  {
    m_pEGL->eglDestroyContext(m_pEGLDisplay, m_pEGLContext);
    m_pEGLContext = nullptr;
  }
  if (m_pEGLDisplay != nullptr)
  {
    m_pEGL->eglTerminate(m_pEGLDisplay);
    m_pEGLDisplay = nullptr;
  }
  m_pEGL = nullptr;
}

bool ezGALDeviceOpenGL::MakeDeviceContextCurrent() const
{
  if (m_pEGL == nullptr || m_pPbufferSurface == nullptr)
    return false;
  return m_pEGL->eglMakeCurrent(m_pEGLDisplay, m_pPbufferSurface, m_pPbufferSurface, m_pEGLContext) != 0;
}

bool ezGALDeviceOpenGL::MakeWindowSurfaceCurrent(void* pWindowSurface) const
{
  if (m_pEGL == nullptr || pWindowSurface == nullptr)
    return false;
  return m_pEGL->eglMakeCurrent(m_pEGLDisplay, pWindowSurface, pWindowSurface, m_pEGLContext) != 0;
}

// #####################################
// ## Command encoder
// #####################################

ezGALCommandEncoder* ezGALDeviceOpenGL::GetCommandEncoder() const
{
  return m_pCommandEncoder.Borrow();
}

ezQueryPoolOpenGL& ezGALDeviceOpenGL::GetQueryPool() const
{
  return *m_pQueryPool;
}

ezGALCommandEncoder* ezGALDeviceOpenGL::BeginCommandsPlatform(const char* szName)
{
  EZ_IGNORE_UNUSED(szName);
  return m_pCommandEncoder.Borrow();
}

void ezGALDeviceOpenGL::EndCommandsPlatform(ezGALCommandEncoder* pPass)
{
  EZ_ASSERT_DEV(m_pCommandEncoder.Borrow() == pPass, "Invalid pass");
  EZ_IGNORE_UNUSED(pPass);
}

void ezGALDeviceOpenGL::FlushPlatform()
{
  if (m_pCommandEncoderImpl)
  {
    m_pCommandEncoderImpl->FlushPlatform();
  }
}

void ezGALDeviceOpenGL::WaitIdlePlatform()
{
  if (m_bGLInitialized)
  {
    MakeDeviceContextCurrent();
    glFinish();
  }
  DestroyDeadObjects();
}

// #####################################
// ## State creation functions
// #####################################

ezGALBlendState* ezGALDeviceOpenGL::CreateBlendStatePlatform(const ezGALBlendStateCreationDescription& Description)
{
  ezGALBlendStateOpenGL* pState = EZ_NEW(&m_Allocator, ezGALBlendStateOpenGL, Description);

  if (pState->InitPlatform(this).Succeeded())
  {
    return pState;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pState);
    return nullptr;
  }
}

void ezGALDeviceOpenGL::DestroyBlendStatePlatform(ezGALBlendState* pBlendState)
{
  ezGALBlendStateOpenGL* pState = static_cast<ezGALBlendStateOpenGL*>(pBlendState);
  pState->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pState);
}

ezGALDepthStencilState* ezGALDeviceOpenGL::CreateDepthStencilStatePlatform(const ezGALDepthStencilStateCreationDescription& Description)
{
  ezGALDepthStencilStateOpenGL* pState = EZ_NEW(&m_Allocator, ezGALDepthStencilStateOpenGL, Description);

  if (pState->InitPlatform(this).Succeeded())
  {
    return pState;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pState);
    return nullptr;
  }
}

void ezGALDeviceOpenGL::DestroyDepthStencilStatePlatform(ezGALDepthStencilState* pDepthStencilState)
{
  ezGALDepthStencilStateOpenGL* pState = static_cast<ezGALDepthStencilStateOpenGL*>(pDepthStencilState);
  pState->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pState);
}

ezGALRasterizerState* ezGALDeviceOpenGL::CreateRasterizerStatePlatform(const ezGALRasterizerStateCreationDescription& Description)
{
  ezGALRasterizerStateOpenGL* pState = EZ_NEW(&m_Allocator, ezGALRasterizerStateOpenGL, Description);

  if (pState->InitPlatform(this).Succeeded())
  {
    return pState;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pState);
    return nullptr;
  }
}

void ezGALDeviceOpenGL::DestroyRasterizerStatePlatform(ezGALRasterizerState* pRasterizerState)
{
  ezGALRasterizerStateOpenGL* pState = static_cast<ezGALRasterizerStateOpenGL*>(pRasterizerState);
  pState->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pState);
}

ezGALSamplerState* ezGALDeviceOpenGL::CreateSamplerStatePlatform(const ezGALSamplerStateCreationDescription& Description)
{
  ezGALSamplerStateOpenGL* pState = EZ_NEW(&m_Allocator, ezGALSamplerStateOpenGL, Description);

  if (pState->InitPlatform(this).Succeeded())
  {
    return pState;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pState);
    return nullptr;
  }
}

void ezGALDeviceOpenGL::DestroySamplerStatePlatform(ezGALSamplerState* pSamplerState)
{
  ezGALSamplerStateOpenGL* pState = static_cast<ezGALSamplerStateOpenGL*>(pSamplerState);
  pState->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pState);
}

void ezGALDeviceOpenGL::RecreateSamplerStatePlatform(ezGALSamplerState* pSamplerState)
{
  ezGALSamplerStateOpenGL* pState = static_cast<ezGALSamplerStateOpenGL*>(pSamplerState);
  pState->DeInitPlatform(this).AssertSuccess();
  pState->InitPlatform(this).AssertSuccess();
}

ezGALBindGroupLayout* ezGALDeviceOpenGL::CreateBindGroupLayoutPlatform(const ezGALBindGroupLayoutCreationDescription& Description)
{
  ezGALBindGroupLayoutOpenGL* pLayout = EZ_NEW(&m_Allocator, ezGALBindGroupLayoutOpenGL, Description);

  if (pLayout->InitPlatform(this).Succeeded())
  {
    return pLayout;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pLayout);
    return nullptr;
  }
}

void ezGALDeviceOpenGL::DestroyBindGroupLayoutPlatform(ezGALBindGroupLayout* pBindGroupLayout)
{
  ezGALBindGroupLayoutOpenGL* pLayout = static_cast<ezGALBindGroupLayoutOpenGL*>(pBindGroupLayout);
  pLayout->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pLayout);
}

ezGALBindGroup* ezGALDeviceOpenGL::CreateBindGroupPlatform(const ezGALBindGroupCreationDescription& Description)
{
  ezGALBindGroupOpenGL* pBindGroup = EZ_NEW(&m_Allocator, ezGALBindGroupOpenGL, Description);

  if (pBindGroup->InitPlatform(this).Succeeded())
  {
    return pBindGroup;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pBindGroup);
    return nullptr;
  }
}

void ezGALDeviceOpenGL::DestroyBindGroupPlatform(ezGALBindGroup* pBindGroup)
{
  ezGALBindGroupOpenGL* pBindGroupOpenGL = static_cast<ezGALBindGroupOpenGL*>(pBindGroup);
  pBindGroupOpenGL->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pBindGroupOpenGL);
}

void ezGALDeviceOpenGL::RecreateBindGroupPlatform(ezGALBindGroup* pBindGroup)
{
  ezGALBindGroupOpenGL* pBindGroupOpenGL = static_cast<ezGALBindGroupOpenGL*>(pBindGroup);
  pBindGroupOpenGL->DeInitPlatform(this).AssertSuccess();
  pBindGroupOpenGL->InitPlatform(this).AssertSuccess();
}

ezGALPipelineLayout* ezGALDeviceOpenGL::CreatePipelineLayoutPlatform(const ezGALPipelineLayoutCreationDescription& Description)
{
  ezGALPipelineLayoutOpenGL* pLayout = EZ_NEW(&m_Allocator, ezGALPipelineLayoutOpenGL, Description);

  if (pLayout->InitPlatform(this).Succeeded())
  {
    return pLayout;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pLayout);
    return nullptr;
  }
}

void ezGALDeviceOpenGL::DestroyPipelineLayoutPlatform(ezGALPipelineLayout* pPipelineLayout)
{
  ezGALPipelineLayoutOpenGL* pLayout = static_cast<ezGALPipelineLayoutOpenGL*>(pPipelineLayout);
  pLayout->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pLayout);
}

ezGALGraphicsPipeline* ezGALDeviceOpenGL::CreateGraphicsPipelinePlatform(const ezGALGraphicsPipelineCreationDescription& Description)
{
  ezGALGraphicsPipelineOpenGL* pPipeline = EZ_NEW(&m_Allocator, ezGALGraphicsPipelineOpenGL, Description);

  if (pPipeline->InitPlatform(this).Succeeded())
  {
    return pPipeline;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pPipeline);
    return nullptr;
  }
}

void ezGALDeviceOpenGL::DestroyGraphicsPipelinePlatform(ezGALGraphicsPipeline* pGraphicsPipeline)
{
  ezGALGraphicsPipelineOpenGL* pPipeline = static_cast<ezGALGraphicsPipelineOpenGL*>(pGraphicsPipeline);
  pPipeline->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pPipeline);
}

ezGALComputePipeline* ezGALDeviceOpenGL::CreateComputePipelinePlatform(const ezGALComputePipelineCreationDescription& Description)
{
  ezGALComputePipelineOpenGL* pPipeline = EZ_NEW(&m_Allocator, ezGALComputePipelineOpenGL, Description);

  if (pPipeline->InitPlatform(this).Succeeded())
  {
    return pPipeline;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pPipeline);
    return nullptr;
  }
}

void ezGALDeviceOpenGL::DestroyComputePipelinePlatform(ezGALComputePipeline* pComputePipeline)
{
  ezGALComputePipelineOpenGL* pPipeline = static_cast<ezGALComputePipelineOpenGL*>(pComputePipeline);
  pPipeline->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pPipeline);
}

// #####################################
// ## Resource creation functions
// #####################################

ezGALShader* ezGALDeviceOpenGL::CreateShaderPlatform(const ezGALShaderCreationDescription& Description)
{
  ezGALShaderOpenGL* pShader = EZ_NEW(&m_Allocator, ezGALShaderOpenGL, Description);

  if (pShader->InitPlatform(this).Succeeded())
  {
    return pShader;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pShader);
    return nullptr;
  }
}

void ezGALDeviceOpenGL::DestroyShaderPlatform(ezGALShader* pShader)
{
  ezGALShaderOpenGL* pShaderOpenGL = static_cast<ezGALShaderOpenGL*>(pShader);
  pShaderOpenGL->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pShaderOpenGL);
}

ezGALBuffer* ezGALDeviceOpenGL::CreateBufferPlatform(const ezGALBufferCreationDescription& Description, ezArrayPtr<const ezUInt8> pInitialData)
{
  ezGALBufferOpenGL* pBuffer = EZ_NEW(&m_Allocator, ezGALBufferOpenGL, Description);

  if (!pBuffer->InitPlatform(this, pInitialData).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pBuffer);
    return nullptr;
  }

  return pBuffer;
}

void ezGALDeviceOpenGL::DestroyBufferPlatform(ezGALBuffer* pBuffer)
{
  ezGALBufferOpenGL* pBufferOpenGL = static_cast<ezGALBufferOpenGL*>(pBuffer);
  pBufferOpenGL->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pBufferOpenGL);
}

ezGALTexture* ezGALDeviceOpenGL::CreateTexturePlatform(const ezGALTextureCreationDescription& Description, ezArrayPtr<ezGALSystemMemoryDescription> pInitialData)
{
  ezGALTextureOpenGL* pTexture = EZ_NEW(&m_Allocator, ezGALTextureOpenGL, Description);

  if (!pTexture->InitPlatform(this, pInitialData).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pTexture);
    return nullptr;
  }

  return pTexture;
}

void ezGALDeviceOpenGL::DestroyTexturePlatform(ezGALTexture* pTexture)
{
  ezGALTextureOpenGL* pTextureOpenGL = static_cast<ezGALTextureOpenGL*>(pTexture);
  pTextureOpenGL->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pTextureOpenGL);
}

ezGALTexture* ezGALDeviceOpenGL::CreateSharedTexturePlatform(const ezGALTextureCreationDescription& Description, ezArrayPtr<ezGALSystemMemoryDescription> pInitialData, ezEnum<ezGALSharedTextureType> sharedType, ezGALPlatformSharedHandle handle)
{
  ezGALSharedTextureOpenGL* pTexture = EZ_NEW(&m_Allocator, ezGALSharedTextureOpenGL, Description, sharedType, handle);

  if (!pTexture->InitPlatform(this, pInitialData).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pTexture);
    return nullptr;
  }

  return pTexture;
}

void ezGALDeviceOpenGL::DestroySharedTexturePlatform(ezGALTexture* pTexture)
{
  ezGALSharedTextureOpenGL* pTextureOpenGL = static_cast<ezGALSharedTextureOpenGL*>(pTexture);
  pTextureOpenGL->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pTextureOpenGL);
}

ezGALReadbackBuffer* ezGALDeviceOpenGL::CreateReadbackBufferPlatform(const ezGALBufferCreationDescription& Description)
{
  ezGALReadbackBufferOpenGL* pBuffer = EZ_NEW(&m_Allocator, ezGALReadbackBufferOpenGL, Description);

  if (!pBuffer->InitPlatform(this).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pBuffer);
    return nullptr;
  }

  return pBuffer;
}

void ezGALDeviceOpenGL::DestroyReadbackBufferPlatform(ezGALReadbackBuffer* pReadbackBuffer)
{
  ezGALReadbackBufferOpenGL* pBufferOpenGL = static_cast<ezGALReadbackBufferOpenGL*>(pReadbackBuffer);
  pBufferOpenGL->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pBufferOpenGL);
}

ezGALReadbackTexture* ezGALDeviceOpenGL::CreateReadbackTexturePlatform(const ezGALTextureCreationDescription& Description)
{
  ezGALReadbackTextureOpenGL* pTexture = EZ_NEW(&m_Allocator, ezGALReadbackTextureOpenGL, Description);

  if (!pTexture->InitPlatform(this).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pTexture);
    return nullptr;
  }

  return pTexture;
}

void ezGALDeviceOpenGL::DestroyReadbackTexturePlatform(ezGALReadbackTexture* pReadbackTexture)
{
  ezGALReadbackTextureOpenGL* pTextureOpenGL = static_cast<ezGALReadbackTextureOpenGL*>(pReadbackTexture);
  pTextureOpenGL->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pTextureOpenGL);
}

ezGALRenderTargetView* ezGALDeviceOpenGL::CreateRenderTargetViewPlatform(ezGALTexture* pTexture, const ezGALRenderTargetViewCreationDescription& Description)
{
  ezGALRenderTargetViewOpenGL* pView = EZ_NEW(&m_Allocator, ezGALRenderTargetViewOpenGL, pTexture, Description);

  if (pView->InitPlatform(this).Succeeded())
  {
    return pView;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pView);
    return nullptr;
  }
}

void ezGALDeviceOpenGL::DestroyRenderTargetViewPlatform(ezGALRenderTargetView* pRenderTargetView)
{
  ezGALRenderTargetViewOpenGL* pViewOpenGL = static_cast<ezGALRenderTargetViewOpenGL*>(pRenderTargetView);
  pViewOpenGL->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pViewOpenGL);
}

// #####################################
// ## Other creation functions
// #####################################

ezGALVertexDeclaration* ezGALDeviceOpenGL::CreateVertexDeclarationPlatform(const ezGALVertexDeclarationCreationDescription& Description)
{
  ezGALVertexDeclarationOpenGL* pVertexDeclaration = EZ_NEW(&m_Allocator, ezGALVertexDeclarationOpenGL, Description);

  if (pVertexDeclaration->InitPlatform(this).Succeeded())
  {
    return pVertexDeclaration;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pVertexDeclaration);
    return nullptr;
  }
}

void ezGALDeviceOpenGL::DestroyVertexDeclarationPlatform(ezGALVertexDeclaration* pVertexDeclaration)
{
  ezGALVertexDeclarationOpenGL* pDeclarationOpenGL = static_cast<ezGALVertexDeclarationOpenGL*>(pVertexDeclaration);
  pDeclarationOpenGL->DeInitPlatform(this).IgnoreResult();
  EZ_DELETE(&m_Allocator, pDeclarationOpenGL);
}

// #####################################
// ## Update functions
// #####################################

void ezGALDeviceOpenGL::UpdateBufferForNextFramePlatform(const ezGALBuffer* pBuffer, ezConstByteArrayPtr sourceData, ezUInt32 uiDestOffset)
{
  // GL has no deferred copy mechanism; perform the update immediately. Transient buffers are orphaned so
  // the GPU is never waiting on data from a previous frame.
  const ezGALBufferOpenGL* pBufferOpenGL = static_cast<const ezGALBufferOpenGL*>(pBuffer);

  if (pBuffer->GetDescription().m_BufferFlags.IsSet(ezGALBufferUsageFlags::Transient))
  {
    glNamedBufferData(pBufferOpenGL->GetGLBuffer(), static_cast<GLsizeiptr>(pBuffer->GetSize()), nullptr, GL_DYNAMIC_DRAW);
  }

  glNamedBufferSubData(pBufferOpenGL->GetGLBuffer(), static_cast<GLintptr>(uiDestOffset), static_cast<GLsizeiptr>(sourceData.GetCount()), sourceData.GetPtr());
}

void ezGALDeviceOpenGL::UpdateTextureForNextFramePlatform(const ezGALTexture* pTexture, const ezGALSystemMemoryDescription& sourceData, const ezGALTextureSubresource& destinationSubResource, const ezBoundingBoxu32& destinationBox)
{
  // Immediate upload; see UpdateBufferForNextFramePlatform.
  const auto* pTextureOpenGL = static_cast<const ezGALTextureOpenGL*>(pTexture->GetParentResource());
  pTextureOpenGL->UploadSubImage(destinationSubResource, destinationBox, sourceData).IgnoreResult();
}

// #####################################
// ## Fences & queries
// #####################################

void ezGALDeviceOpenGL::RegisterFence(GLsync pFence)
{
  if (pFence != nullptr)
  {
    m_Fences.PushBack(pFence);
  }
}

ezGALFenceHandle ezGALDeviceOpenGL::SubmitFence()
{
  GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  if (fence == nullptr)
  {
    ezLog::Error("Failed to create GL fence sync object");
    return 0;
  }
  RegisterFence(fence);
  return reinterpret_cast<ezUInt64>(fence);
}

static void EraseFenceInternal(ezDynamicArray<GLsync>& fences, GLsync fence)
{
  for (ezUInt32 i = 0; i < fences.GetCount(); ++i)
  {
    if (fences[i] == fence)
    {
      fences.RemoveAtAndSwap(i);
      return;
    }
  }
}

void ezGALDeviceOpenGL::ReclaimFinishedFrames()
{
  for (ezUInt32 uiFrame = m_uiSafeFrame + 1; uiFrame < m_uiFrameCounter; ++uiFrame)
  {
    PerFrameData& perFrameData = m_PerFrameData[uiFrame % FRAMES];
    if (perFrameData.m_hFence == 0 || perFrameData.m_uiFrame != uiFrame)
      continue;

    GLsync fence = reinterpret_cast<GLsync>(perFrameData.m_hFence);
    const GLenum res = glClientWaitSync(fence, 0, 0);
    if (res == GL_ALREADY_SIGNALED || res == GL_CONDITION_SATISFIED)
    {
      EraseFenceInternal(m_Fences, fence);
      glDeleteSync(fence);
      perFrameData.m_hFence = {};
      perFrameData.m_uiFrame = ezUInt64(-1);
      m_uiSafeFrame = uiFrame;
    }
    else
    {
      break;
    }
  }
}

ezEnum<ezGALAsyncResult> ezGALDeviceOpenGL::GetTimestampResultPlatform(ezGALTimestampHandle hTimestamp, ezTime& out_result)
{
  if (m_pQueryPool == nullptr)
    return ezGALAsyncResult::Expired;
  return m_pQueryPool->GetTimestampResult(hTimestamp, out_result);
}

ezEnum<ezGALAsyncResult> ezGALDeviceOpenGL::GetOcclusionResultPlatform(ezGALOcclusionHandle hOcclusion, ezUInt64& out_uiResult)
{
  if (m_pQueryPool == nullptr)
    return ezGALAsyncResult::Expired;
  return m_pQueryPool->GetOcclusionQueryResult(hOcclusion, out_uiResult);
}

ezEnum<ezGALAsyncResult> ezGALDeviceOpenGL::GetFenceResultPlatform(ezGALFenceHandle hFence, ezTime timeout)
{
  if (hFence == 0)
    return ezGALAsyncResult::Expired;

  // The fence might not have been submitted to the GPU yet; flushing here guarantees
  // that a waiting caller with a timeout does not deadlock.
  if (timeout.IsPositive())
  {
    FlushPlatform();
  }

  if (!m_bGLInitialized)
    return ezGALAsyncResult::Expired;

  GLsync fence = reinterpret_cast<GLsync>(hFence);
  const GLuint64 uiTimeoutNs = timeout.IsPositive() ? static_cast<GLuint64>(timeout.GetNanoseconds()) : 0;
  const GLbitfield flags = timeout.IsPositive() ? GL_SYNC_FLUSH_COMMANDS_BIT : 0;

  const GLenum res = glClientWaitSync(fence, flags, uiTimeoutNs);

  // Fences can be queried multiple times (e.g. GetReadbackResult() followed by LockTexture() re-queries the
  // same fence), so a fence must remain valid after it was signaled. Sync objects stay alive until glDeleteSync;
  // only frame-submit fences are reclaimed in ReclaimFinishedFrames(), everything else lives until shutdown.
  if (res == GL_ALREADY_SIGNALED || res == GL_CONDITION_SATISFIED)
  {
    return ezGALAsyncResult::Ready;
  }
  else if (res == GL_TIMEOUT_EXPIRED)
  {
    return ezGALAsyncResult::Expired;
  }

  EZ_REPORT_FAILURE("glClientWaitSync failed with GL_WAIT_FAILED");
  return ezGALAsyncResult::Expired;
}

ezResult ezGALDeviceOpenGL::LockBufferPlatform(const ezGALReadbackBuffer* pBuffer, ezArrayPtr<const ezUInt8>& out_memory) const
{
  const ezGALReadbackBufferOpenGL* pBufferOpenGL = static_cast<const ezGALReadbackBufferOpenGL*>(pBuffer);
  const ezUInt32 uiSize = pBuffer->GetDescription().m_uiTotalSize;

  const void* pMapped = glMapNamedBufferRange(pBufferOpenGL->GetGLBuffer(), 0, static_cast<GLsizeiptr>(uiSize), GL_MAP_READ_BIT);
  if (pMapped == nullptr)
  {
    ezLog::Error("Failed to map the readback buffer");
    return EZ_FAILURE;
  }

  out_memory = ezArrayPtr<const ezUInt8>(static_cast<const ezUInt8*>(pMapped), uiSize);
  return EZ_SUCCESS;
}

void ezGALDeviceOpenGL::UnlockBufferPlatform(const ezGALReadbackBuffer* pBuffer) const
{
  const ezGALReadbackBufferOpenGL* pBufferOpenGL = static_cast<const ezGALReadbackBufferOpenGL*>(pBuffer);
  glUnmapNamedBuffer(pBufferOpenGL->GetGLBuffer());
}

namespace
{
  struct ezGALReadbackLayout
  {
    ezUInt32 m_uiRowPitch = 0;
    ezUInt32 m_uiSlicePitch = 0;
    ezUInt32 m_uiTotalSize = 0;
  };

  ezResult ComputeReadbackLayout(const ezGALTextureCreationDescription& desc, ezUInt32 uiMipLevel, ezGALReadbackLayout& out_layout)
  {
    const ezUInt32 uiWidth = ezMath::Max<ezUInt32>(1, desc.m_uiWidth >> uiMipLevel);
    const ezUInt32 uiHeight = ezMath::Max<ezUInt32>(1, desc.m_uiHeight >> uiMipLevel);
    const ezUInt32 uiDepth = ezMath::Max<ezUInt32>(1, desc.m_uiDepth >> uiMipLevel);

    ezUInt32 uiBlockBytes = 0;
    if (ezMappingTables::IsBlockCompressedFormat(desc.m_Format, uiBlockBytes))
    {
      const ezUInt32 uiBlockWidth = 4;
      const ezUInt32 uiBlockHeight = 4;
      const ezUInt32 uiBlocksX = (uiWidth + uiBlockWidth - 1) / uiBlockWidth;
      const ezUInt32 uiBlocksY = (uiHeight + uiBlockHeight - 1) / uiBlockHeight;
      out_layout.m_uiRowPitch = uiBlocksX * uiBlockBytes;
      out_layout.m_uiSlicePitch = out_layout.m_uiRowPitch * uiBlocksY;
      out_layout.m_uiTotalSize = out_layout.m_uiSlicePitch * uiDepth;
      return EZ_SUCCESS;
    }

    const ezUInt32 uiBitsPerElement = ezGALResourceFormat::GetBitsPerElement(desc.m_Format);
    if (uiBitsPerElement == 0)
      return EZ_FAILURE;

    const ezUInt32 uiBytesPerElement = uiBitsPerElement / 8;
    out_layout.m_uiRowPitch = uiWidth * uiBytesPerElement;
    out_layout.m_uiSlicePitch = out_layout.m_uiRowPitch * uiHeight;
    out_layout.m_uiTotalSize = out_layout.m_uiSlicePitch * uiDepth;
    return EZ_SUCCESS;
  }
} // namespace

ezResult ezGALDeviceOpenGL::LockTexturePlatform(const ezGALReadbackTexture* pTexture, const ezArrayPtr<const ezGALTextureSubresource>& subResources, ezDynamicArray<ezGALSystemMemoryDescription>& out_memory) const
{
  const ezGALReadbackTextureOpenGL* pTextureOpenGL = static_cast<const ezGALReadbackTextureOpenGL*>(pTexture);
  const ezGALTextureCreationDescription& desc = pTexture->GetDescription();

  const GLenum eFormat = ezMappingTables::ToGLTextureFormat(desc.m_Format);
  const GLenum eType = ezMappingTables::ToGLTextureType(desc.m_Format);

  ezUInt32 uiBlockBytes = 0;
  const bool bCompressed = ezMappingTables::IsBlockCompressedFormat(desc.m_Format, uiBlockBytes);
  if (!bCompressed && (eFormat == 0 || eType == 0))
  {
    ezLog::Error("Unsupported format {0} for texture readback.", desc.m_Format.GetValue());
    return EZ_FAILURE;
  }

  out_memory.SetCount(subResources.GetCount());

  for (ezUInt32 uiIndex = 0; uiIndex < subResources.GetCount(); ++uiIndex)
  {
    const ezGALTextureSubresource& subResource = subResources[uiIndex];

    ezGALReadbackLayout layout;
    if (ComputeReadbackLayout(desc, subResource.m_uiMipLevel, layout).Failed())
    {
      ezLog::Error("Failed to compute the readback layout for mip {0}.", subResource.m_uiMipLevel);
      for (void* p : pTextureOpenGL->m_MappedAllocations)
      {
        free(p);
      }
      pTextureOpenGL->m_MappedAllocations.Clear();
      return EZ_FAILURE;
    }

    void* pData = malloc(layout.m_uiTotalSize);
    if (pData == nullptr)
      return EZ_FAILURE;

    const ezUInt32 uiWidth = ezMath::Max<ezUInt32>(1, desc.m_uiWidth >> subResource.m_uiMipLevel);
    const ezUInt32 uiHeight = ezMath::Max<ezUInt32>(1, desc.m_uiHeight >> subResource.m_uiMipLevel);
    const ezUInt32 uiDepth = desc.m_Type == ezGALTextureType::Texture3D ? ezMath::Max<ezUInt32>(1, desc.m_uiDepth >> subResource.m_uiMipLevel) : 1;

    if (bCompressed)
    {
      glGetCompressedTextureSubImage(pTextureOpenGL->GetGLTexture(), static_cast<GLint>(subResource.m_uiMipLevel),
        0, 0, static_cast<GLint>(subResource.m_uiArraySlice),
        static_cast<GLsizei>(uiWidth), static_cast<GLsizei>(uiHeight), static_cast<GLsizei>(uiDepth),
        static_cast<GLsizei>(layout.m_uiTotalSize), pData);
    }
    else
    {
      glGetTextureSubImage(pTextureOpenGL->GetGLTexture(), static_cast<GLint>(subResource.m_uiMipLevel),
        0, 0, static_cast<GLint>(subResource.m_uiArraySlice),
        static_cast<GLsizei>(uiWidth), static_cast<GLsizei>(uiHeight), static_cast<GLsizei>(uiDepth),
        eFormat, eType, static_cast<GLsizei>(layout.m_uiTotalSize), pData);
    }

    pTextureOpenGL->m_MappedAllocations.PushBack(pData);

    ezGALSystemMemoryDescription& memDesc = out_memory[uiIndex];
    memDesc.m_pData = ezConstByteBlobPtr(static_cast<const ezUInt8*>(pData), layout.m_uiTotalSize);
    memDesc.m_uiRowPitch = layout.m_uiRowPitch;
    memDesc.m_uiSlicePitch = layout.m_uiSlicePitch;
  }

  return EZ_SUCCESS;
}

void ezGALDeviceOpenGL::UnlockTexturePlatform(const ezGALReadbackTexture* pTexture, const ezArrayPtr<const ezGALTextureSubresource>& subResources) const
{
  EZ_IGNORE_UNUSED(subResources);
  const ezGALReadbackTextureOpenGL* pTextureOpenGL = static_cast<const ezGALReadbackTextureOpenGL*>(pTexture);

  for (void* p : pTextureOpenGL->m_MappedAllocations)
  {
    free(p);
  }
  pTextureOpenGL->m_MappedAllocations.Clear();
}

// #####################################
// ## Frame management
// #####################################

void ezGALDeviceOpenGL::BeginFramePlatform(ezArrayPtr<ezGALSwapChain*> swapchains, const ezUInt64 uiAppFrame)
{
  EZ_IGNORE_UNUSED(swapchains);
  EZ_IGNORE_UNUSED(uiAppFrame);

  ReclaimFinishedFrames();

  EZ_ASSERT_DEBUG((m_uiFrameCounter % FRAMES) == m_uiCurrentPerFrameData, "");
  m_PerFrameData[m_uiCurrentPerFrameData].m_uiFrame = m_uiFrameCounter;

  m_pQueryPool->BeginFrame();
}

void ezGALDeviceOpenGL::EndFramePlatform(ezArrayPtr<ezGALSwapChain*> swapchains)
{
  for (ezGALSwapChain* pSwapChain : swapchains)
  {
    pSwapChain->PresentRenderTarget(this);
  }

  m_pQueryPool->EndFrame();

  auto& currentFrameData = m_PerFrameData[m_uiCurrentPerFrameData];
  currentFrameData.m_hFence = SubmitFence();

  ++m_uiFrameCounter;
  m_uiCurrentPerFrameData = static_cast<ezUInt8>(m_uiFrameCounter % FRAMES);
}

ezUInt64 ezGALDeviceOpenGL::GetCurrentFramePlatform() const
{
  return m_uiFrameCounter;
}

ezUInt64 ezGALDeviceOpenGL::GetSafeFramePlatform() const
{
  return m_uiSafeFrame;
}

// #####################################
// ## Capabilities
// #####################################

void ezGALDeviceOpenGL::FillCapabilitiesPlatform()
{
  const GLubyte* szRenderer = glGetString(GL_RENDERER);
  const GLubyte* szVendor = glGetString(GL_VENDOR);
  m_Capabilities.m_sAdapterName = szRenderer != nullptr ? reinterpret_cast<const char*>(szRenderer) : "OpenGL";
  if (szVendor != nullptr)
  {
    ezStringBuilder tmp;
    const char* szRendererName = szRenderer != nullptr ? reinterpret_cast<const char*>(szRenderer) : "OpenGL";
    tmp.SetFormat("{0} ({1})", szRendererName, reinterpret_cast<const char*>(szVendor));
    m_Capabilities.m_sAdapterName = tmp;
  }
  m_Capabilities.m_bHardwareAccelerated = true;
  m_Capabilities.m_uiDedicatedVRAM = 0; // no portable way to query VRAM in GL
  m_Capabilities.m_uiDedicatedSystemRAM = 0;
  m_Capabilities.m_uiSharedSystemRAM = 0;

  m_Capabilities.m_bSupportsMultithreadedResourceCreation = false;
  m_Capabilities.m_bSupportsMultipleBindGroups = true;
  m_Capabilities.m_materialBufferLayout = ezGALBufferLayout::Vulkan_Std430_relaxed;

  for (ezUInt32 i = 0; i < ezGALShaderStage::ENUM_COUNT; ++i)
  {
    m_Capabilities.m_bShaderStageSupported[i] = true;
  }
  m_Capabilities.m_bSupportsIndirectDraw = true;
  m_Capabilities.m_bSupportsConservativeRasterization = false;
  m_Capabilities.m_bSupportsDepthBiasClamp = false; // GL has no constant depth bias clamp
  m_Capabilities.m_bSupportsWireframe = true;       // glPolygonMode(GL_LINE)
  m_Capabilities.m_bSupportsVSRenderTargetArrayIndex = false;
  m_Capabilities.m_bSupportsTexelBuffer = true;
  m_Capabilities.m_bSupportsMultipleSRVTypes = true;
  m_Capabilities.m_bSupportsMultiSampledArrays = true;
  m_Capabilities.m_uiMaxPushConstantsSize = 256; // size of the encoder's dedicated push-constant UBO

  // Fill the format support table based on which internal formats are valid for this context.
  m_Capabilities.m_FormatSupport.SetCount(ezGALResourceFormat::ENUM_COUNT);
  for (ezUInt32 i = 0; i < ezGALResourceFormat::ENUM_COUNT; i++)
  {
    const ezGALResourceFormat::Enum format = (ezGALResourceFormat::Enum)i;
    const GLenum eInternalFormat = ezMappingTables::ToGLTextureInternalFormat(format);
    if (eInternalFormat == 0)
      continue;

    ezBitflags<ezGALResourceFormatSupport> support;
    support.Add(ezGALResourceFormatSupport::Texture);

    ezUInt32 uiBlockBytes = 0;
    if (!ezMappingTables::IsBlockCompressedFormat(format, uiBlockBytes))
    {
      // Vertex attributes can only use uncompressed formats
      support.Add(ezGALResourceFormatSupport::VertexAttribute);
    }

    // Color and depth formats can be used as render targets; GL only validates this at FBO completeness time.
    support.Add(ezGALResourceFormatSupport::RenderTarget);

    // Query MSAA sample counts for 2D multisample targets
    GLint iSamplesCount = 0;
    GLint samples[16] = {};
    glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE, eInternalFormat, GL_NUM_SAMPLE_COUNTS, 1, &iSamplesCount);
    if (iSamplesCount > 0)
    {
      glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE, eInternalFormat, GL_SAMPLES, ezMath::Min<GLint>(iSamplesCount, 16), samples);
      for (GLint s = 0; s < ezMath::Min<GLint>(iSamplesCount, 16); ++s)
      {
        if (samples[s] == 2)
          support.Add(ezGALResourceFormatSupport::MSAA2x);
        else if (samples[s] == 4)
          support.Add(ezGALResourceFormatSupport::MSAA4x);
        else if (samples[s] == 8)
          support.Add(ezGALResourceFormatSupport::MSAA8x);
      }
    }

    // Image load/store is guaranteed for the basic uncompressed formats; be conservative and only advertise those.
    if (ezMappingTables::IsImageLoadStoreSupportedFormat(format))
    {
      support.Add(ezGALResourceFormatSupport::TextureRW);
    }

    m_Capabilities.m_FormatSupport[i] = support;
  }
}

const ezGALSharedTexture* ezGALDeviceOpenGL::GetSharedTexture(ezGALTextureHandle hTexture) const
{
  auto pTexture = GetTexture(hTexture);
  if (pTexture == nullptr)
  {
    return nullptr;
  }
  // Resolve proxy texture if any
  return static_cast<const ezGALSharedTextureOpenGL*>(pTexture->GetParentResource());
}
