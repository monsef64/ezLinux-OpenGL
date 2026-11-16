#include <RendererOpenGL/Pools/QueryPoolOpenGL.h>
#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <Foundation/Math/Rect.h>
#include <Foundation/Types/ScopeExit.h>

#include <RendererFoundation/CommandEncoder/CommandEncoder.h>
#include <RendererFoundation/Descriptors/Descriptors.h>
#include <RendererFoundation/Descriptors/Enumerations.h>
#include <RendererFoundation/Resources/Buffer.h>
#include <RendererFoundation/Resources/ReadbackBuffer.h>
#include <RendererFoundation/Resources/ReadbackTexture.h>
#include <RendererFoundation/Resources/RenderTargetView.h>
#include <RendererFoundation/Resources/Texture.h>
#include <RendererFoundation/Shader/BindGroup.h>
#include <RendererFoundation/Shader/BindGroupLayout.h>
#include <RendererFoundation/Shader/ShaderByteCode.h>
#include <RendererFoundation/State/ComputePipeline.h>
#include <RendererFoundation/State/GraphicsPipeline.h>
#include <RendererFoundation/State/State.h>

#include <RendererOpenGL/CommandEncoder/CommandEncoderImplOpenGL.h>
#include <RendererOpenGL/Device/DeviceOpenGL.h>
#include <RendererOpenGL/Resources/BufferOpenGL.h>
#include <RendererOpenGL/Resources/ReadbackBufferOpenGL.h>
#include <RendererOpenGL/Resources/ReadbackTextureOpenGL.h>
#include <RendererOpenGL/Resources/RenderTargetViewOpenGL.h>
#include <RendererOpenGL/Resources/TextureOpenGL.h>
#include <RendererOpenGL/Shader/ShaderOpenGL.h>
#include <RendererOpenGL/Shader/VertexDeclarationOpenGL.h>
#include <RendererOpenGL/State/PipelinesOpenGL.h>
#include <RendererOpenGL/State/StateOpenGL.h>
#include <RendererOpenGL/Utils/MappingTablesOpenGL.h>

static constexpr ezUInt32 EZ_GL_PUSH_CONSTANT_BUFFER_START_SIZE = 256;

ezGALCommandEncoderImplOpenGL::ezGALCommandEncoderImplOpenGL(ezGALDeviceOpenGL& ref_device)
  : m_DeviceOpenGL(ref_device)
{
  // Static arrays are empty until EnsureCount() is called; indexing them before that is out-of-bounds.
  m_BoundVertexBuffers.EnsureCount(EZ_GAL_MAX_VERTEX_BUFFER_COUNT);
  m_VertexBufferOffsets.EnsureCount(EZ_GAL_MAX_VERTEX_BUFFER_COUNT);
  for (ezUInt32 i = 0; i < EZ_GAL_MAX_VERTEX_BUFFER_COUNT; i++)
  {
    m_BoundVertexBuffers[i] = 0;
    m_VertexBufferOffsets[i] = 0;
  }
}

ezGALCommandEncoderImplOpenGL::~ezGALCommandEncoderImplOpenGL()
{
  for (auto& fbo : m_FBOCache)
  {
    if (fbo.m_uiFBO != 0)
      glDeleteFramebuffers(1, &fbo.m_uiFBO);
  }

  for (GLuint& uiFBO : m_uiResolveFBO)
  {
    if (uiFBO != 0)
      glDeleteFramebuffers(1, &uiFBO);
  }

  for (GLuint& uiFBO : m_uiCopyFBO)
  {
    if (uiFBO != 0)
      glDeleteFramebuffers(1, &uiFBO);
  }

  if (m_uiPushConstantBuffer != 0)
  {
    glDeleteBuffers(1, &m_uiPushConstantBuffer);
    m_uiPushConstantBuffer = 0;
  }

  if (m_uiVAO != 0)
  {
    glDeleteVertexArrays(1, &m_uiVAO);
    m_uiVAO = 0;
  }
}

void ezGALCommandEncoderImplOpenGL::InitializeVAO()
{
  if (m_uiVAO != 0)
    return;

  glGenVertexArrays(1, &m_uiVAO);
  glBindVertexArray(m_uiVAO);
  m_bVertexStateDirty = true;
}

void ezGALCommandEncoderImplOpenGL::InitializePushConstantBuffer()
{
  if (m_uiPushConstantBuffer != 0)
    return;

  glGenBuffers(1, &m_uiPushConstantBuffer);
  glBindBuffer(GL_UNIFORM_BUFFER, m_uiPushConstantBuffer);
  glBufferData(GL_UNIFORM_BUFFER, EZ_GL_PUSH_CONSTANT_BUFFER_START_SIZE, nullptr, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

// #####################################
// ## Resource binding functions
// #####################################

void ezGALCommandEncoderImplOpenGL::ApplyBindGroup(ezUInt32 uiBindGroup, const ezGALBindGroupCreationDescription& bindGroup)
{
  EZ_IGNORE_UNUSED(uiBindGroup);

  const ezGALBindGroupLayout* pLayout = m_DeviceOpenGL.GetBindGroupLayout(bindGroup.m_hBindGroupLayout);
  if (pLayout == nullptr)
    return;

  ezArrayPtr<const ezShaderResourceBinding> bindings = pLayout->GetDescription().m_ResourceBindings;
  const ezUInt32 uiBindingCount = bindings.GetCount();

  for (ezUInt32 i = 0; i < uiBindingCount && i < bindGroup.m_BindGroupItems.GetCount(); ++i)
  {
    const ezShaderResourceBinding& binding = bindings[i];
    const ezGALBindGroupItem& item = bindGroup.m_BindGroupItems[i];
    const ezUInt32 uiKey = MakeBindKey(static_cast<ezUInt32>(binding.m_iBindGroup), static_cast<ezUInt32>(binding.m_iSlot));
    const bool bEmpty = item.m_Flags.IsSet(ezGALBindGroupItemFlags::EmptyBinding);

    switch (binding.m_ResourceType)
    {
      case ezGALShaderResourceType::ConstantBuffer:
      {
        m_BoundUniformBuffers.Insert(uiKey, bEmpty ? ezGALBufferHandle() : item.m_Buffer.m_hBuffer);
        m_bResourceStateDirty = true;
        break;
      }

      case ezGALShaderResourceType::StructuredBuffer:
      case ezGALShaderResourceType::ByteAddressBuffer:
      case ezGALShaderResourceType::TexelBuffer:
      case ezGALShaderResourceType::StructuredBufferRW:
      case ezGALShaderResourceType::ByteAddressBufferRW:
      case ezGALShaderResourceType::TexelBufferRW:
      {
        BoundBufferRange range;
        range.m_hBuffer = bEmpty ? ezGALBufferHandle() : item.m_Buffer.m_hBuffer;
        range.m_uiOffset = item.m_Buffer.m_BufferRange.m_uiByteOffset;
        range.m_uiSize = item.m_Buffer.m_BufferRange.m_uiByteCount == EZ_GAL_WHOLE_SIZE ? ezUInt32(0xFFFFFFFFu) : item.m_Buffer.m_BufferRange.m_uiByteCount;
        m_BoundStorageBuffers.Insert(uiKey, range);
        m_bResourceStateDirty = true;
        break;
      }

      case ezGALShaderResourceType::Texture:
      {
        BoundTexture bound;
        bound.m_hTexture = bEmpty ? ezGALTextureHandle() : item.m_Texture.m_hTexture;
        bound.m_Range = item.m_Texture.m_TextureRange;
        m_BoundTextures.Insert(uiKey, bound);
        m_bResourceStateDirty = true;
        break;
      }

      case ezGALShaderResourceType::TextureRW:
      {
        BoundImage bound;
        bound.m_hTexture = bEmpty ? ezGALTextureHandle() : item.m_Texture.m_hTexture;
        bound.m_uiLevel = item.m_Texture.m_TextureRange.m_uiBaseMipLevel;
        bound.m_uiLayer = item.m_Texture.m_TextureRange.m_uiBaseArraySlice;
        bound.m_bLayered = false;
        if (!bound.m_hTexture.IsInvalidated())
        {
          // The texture is guaranteed to be alive while its bind group is valid, so reading the description here is safe.
          if (const ezGALTexture* pTexture = m_DeviceOpenGL.GetTexture(bound.m_hTexture))
          {
            if (const ezGALTextureOpenGL* pPlatformTexture = ResolvePlatformTexture(pTexture))
            {
              const auto& desc = pPlatformTexture->GetDescription();
              bound.m_bLayered = desc.m_Type == ezGALTextureType::Texture2DArray || desc.m_Type == ezGALTextureType::TextureCubeArray || desc.m_Type == ezGALTextureType::Texture3D;
            }
          }
        }
        m_BoundImages.Insert(uiKey, bound);
        m_bResourceStateDirty = true;
        break;
      }

      case ezGALShaderResourceType::Sampler:
      {
        m_BoundSamplers.Insert(uiKey, bEmpty ? ezGALSamplerStateHandle() : item.m_Sampler.m_hSampler);
        m_bResourceStateDirty = true;
        break;
      }

      case ezGALShaderResourceType::TextureAndSampler:
      {
        BoundTexture bound;
        bound.m_hTexture = bEmpty ? ezGALTextureHandle() : item.m_Texture.m_hTexture;
        bound.m_Range = item.m_Texture.m_TextureRange;
        m_BoundTextures.Insert(uiKey, bound);

        m_BoundSamplers.Insert(uiKey, bEmpty ? ezGALSamplerStateHandle() : item.m_Texture.m_hSampler);
        m_bResourceStateDirty = true;
        break;
      }

      default:
        ezLog::Warning("Resource type {0} is not supported by the OpenGL backend yet.", static_cast<ezUInt8>(binding.m_ResourceType));
        break;
    }
  }
}

void ezGALCommandEncoderImplOpenGL::SetBindGroupPlatform(ezUInt32 uiBindGroup, const ezGALBindGroupCreationDescription& bindGroup)
{
  ApplyBindGroup(uiBindGroup, bindGroup);
}

void ezGALCommandEncoderImplOpenGL::SetBindGroupPlatform(ezUInt32 uiBindGroup, const ezGALBindGroup* pBindGroup)
{
  ApplyBindGroup(uiBindGroup, pBindGroup->GetDescription());
}

void ezGALCommandEncoderImplOpenGL::SetPushConstantsPlatform(ezArrayPtr<const ezUInt8> data)
{
  InitializePushConstantBuffer();

  glBindBuffer(GL_UNIFORM_BUFFER, m_uiPushConstantBuffer);
  glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(data.GetCount()), data.GetPtr(), GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

// #####################################
// ## Deferred state flush
// #####################################

static const ezGALShaderOpenGL* GetCurrentGLShader(
  const ezGALDeviceOpenGL& device, bool bComputePipelineSet, const ezGALComputePipelineCreationDescription& computeDesc, bool bGraphicsPipelineSet, const ezGALGraphicsPipelineCreationDescription& gfxDesc)
{
  const ezGALShaderHandle hShader = bComputePipelineSet ? computeDesc.m_hShader : (bGraphicsPipelineSet ? gfxDesc.m_hShader : ezGALShaderHandle());
  if (hShader.IsInvalidated())
    return nullptr;
  return static_cast<const ezGALShaderOpenGL*>(device.GetShader(hShader));
}

void ezGALCommandEncoderImplOpenGL::FlushDeferredResourceState()
{
  if (!m_bResourceStateDirty)
    return;

  const ezGALShaderOpenGL* pShader = GetCurrentGLShader(m_DeviceOpenGL, m_bComputePipelineSet, m_BoundComputePipelineDesc, m_bGraphicsPipelineSet, m_BoundGraphicsPipelineDesc);
  if (pShader == nullptr)
    return;

  // Constant buffers
  for (auto it = m_BoundUniformBuffers.GetIterator(); it.IsValid(); ++it)
  {
    const ezUInt32 uiKey = it.Key();
    const ezUInt32 uiSet = uiKey >> 16;
    const ezUInt32 uiSlot = uiKey & 0xFFFFu;
    const ezUInt32 uiFlat = pShader->GetFlatBinding(ezGLBindingClass::UniformBuffer, static_cast<ezInt16>(uiSet), static_cast<ezInt16>(uiSlot));
    if (uiFlat == 0xFFFFFFFFu)
      continue;

    const ezGALBufferHandle hBuffer = it.Value();
    const ezGALBufferOpenGL* pBuffer = hBuffer.IsInvalidated() ? nullptr : static_cast<const ezGALBufferOpenGL*>(m_DeviceOpenGL.GetBuffer(hBuffer));
    glBindBufferBase(GL_UNIFORM_BUFFER, uiFlat, pBuffer != nullptr ? pBuffer->GetGLBuffer() : 0);
  }

  // Structured / byte address / texel buffers
  for (auto it = m_BoundStorageBuffers.GetIterator(); it.IsValid(); ++it)
  {
    const ezUInt32 uiKey = it.Key();
    const ezUInt32 uiSet = uiKey >> 16;
    const ezUInt32 uiSlot = uiKey & 0xFFFFu;
    const ezUInt32 uiFlat = pShader->GetFlatBinding(ezGLBindingClass::StorageBuffer, static_cast<ezInt16>(uiSet), static_cast<ezInt16>(uiSlot));
    if (uiFlat == 0xFFFFFFFFu)
      continue;

    const BoundBufferRange& range = it.Value();
    // The buffer may have been destroyed since the bind group was validated; handles resolve to nullptr in that case.
    const ezGALBufferOpenGL* pBuffer = range.m_hBuffer.IsInvalidated() ? nullptr : static_cast<const ezGALBufferOpenGL*>(m_DeviceOpenGL.GetBuffer(range.m_hBuffer));
    const GLuint uiBuffer = pBuffer != nullptr ? pBuffer->GetGLBuffer() : 0;
    const ezUInt32 uiOffset = range.m_uiOffset;
    if (range.m_uiSize == 0xFFFFFFFFu || pBuffer == nullptr)
    {
      if (uiBuffer != 0 && range.m_uiSize == 0xFFFFFFFFu)
      {
        // Full buffer binding. Byte address ranges m_uiByteCount/m_uiByteOffset still apply here.
        if (uiOffset == 0)
          glBindBufferBase(GL_SHADER_STORAGE_BUFFER, uiFlat, uiBuffer);
        else
          glBindBufferRange(GL_SHADER_STORAGE_BUFFER, uiFlat, uiBuffer, uiOffset, pBuffer->GetSize() - uiOffset);
      }
      else
      {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, uiFlat, 0);
      }
    }
    else
    {
      glBindBufferRange(GL_SHADER_STORAGE_BUFFER, uiFlat, uiBuffer, uiOffset, range.m_uiSize);
    }
  }

  // Storage images (TextureRW)
  for (auto it = m_BoundImages.GetIterator(); it.IsValid(); ++it)
  {
    const ezUInt32 uiKey = it.Key();
    const ezUInt32 uiSet = uiKey >> 16;
    const ezUInt32 uiSlot = uiKey & 0xFFFFu;
    const ezUInt32 uiFlat = pShader->GetFlatBinding(ezGLBindingClass::ImageUnit, static_cast<ezInt16>(uiSet), static_cast<ezInt16>(uiSlot));
    if (uiFlat == 0xFFFFFFFFu)
      continue;

    const BoundImage& image = it.Value();
    const ezGALTexture* pTextureBase = image.m_hTexture.IsInvalidated() ? nullptr : m_DeviceOpenGL.GetTexture(image.m_hTexture);
    const ezGALTextureOpenGL* pTexture = pTextureBase != nullptr ? ResolvePlatformTexture(pTextureBase) : nullptr;
    if (pTexture == nullptr)
    {
      glBindImageTexture(uiFlat, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
      continue;
    }

    const ezGALTextureCreationDescription& desc = pTexture->GetDescription();
    GLenum eFormat = ezMappingTables::GetImageFormat(desc.m_Format);
    if (eFormat == 0)
    {
      ezLog::Error("Texture format {0} is not supported as a storage image.", desc.m_Format.GetValue());
      eFormat = GL_RGBA8;
    }

    const GLuint uiLayered = image.m_bLayered ? GL_TRUE : GL_FALSE;
    glBindImageTexture(uiFlat, pTexture->GetGLTexture(), static_cast<GLint>(image.m_uiLevel), static_cast<GLboolean>(uiLayered), static_cast<GLint>(image.m_uiLayer), GL_READ_WRITE, eFormat);
  }

  // Sampled textures
  for (auto it = m_BoundTextures.GetIterator(); it.IsValid(); ++it)
  {
    const ezUInt32 uiKey = it.Key();
    const ezUInt32 uiSet = uiKey >> 16;
    const ezUInt32 uiSlot = uiKey & 0xFFFFu;
    const ezUInt32 uiFlat = pShader->GetFlatBinding(ezGLBindingClass::ImageUnit, static_cast<ezInt16>(uiSet), static_cast<ezInt16>(uiSlot));
    if (uiFlat == 0xFFFFFFFFu)
      continue;

    const BoundTexture& bound = it.Value();
    glActiveTexture(GL_TEXTURE0 + uiFlat);
    const ezGALTexture* pTextureBase = bound.m_hTexture.IsInvalidated() ? nullptr : m_DeviceOpenGL.GetTexture(bound.m_hTexture);
    const ezGALTextureOpenGL* pTexture = pTextureBase != nullptr ? ResolvePlatformTexture(pTextureBase) : nullptr;
    if (pTexture == nullptr)
    {
      glBindTexture(GL_TEXTURE_2D, 0);
      continue;
    }

    glBindTexture(pTexture->GetTarget(), pTexture->GetGLTexture());

    // Restrict the mip range to the bound view. Only needed for partial mip ranges.
    const ezUInt8 uiBaseMip = bound.m_Range.m_uiBaseMipLevel;
    const ezUInt8 uiMipLevels = bound.m_Range.m_uiMipLevels;
    if (uiBaseMip != 0 || uiMipLevels != EZ_GAL_ALL_MIP_LEVELS)
    {
      const ezUInt32 uiLastMip = uiBaseMip + (uiMipLevels == EZ_GAL_ALL_MIP_LEVELS ? pTexture->GetDescription().m_uiMipLevelCount : uiMipLevels) - 1;
      glTexParameteri(pTexture->GetTarget(), GL_TEXTURE_BASE_LEVEL, uiBaseMip);
      glTexParameteri(pTexture->GetTarget(), GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(uiLastMip));
    }
  }

  // Samplers
  for (auto it = m_BoundSamplers.GetIterator(); it.IsValid(); ++it)
  {
    const ezUInt32 uiKey = it.Key();
    const ezUInt32 uiSet = uiKey >> 16;
    const ezUInt32 uiSlot = uiKey & 0xFFFFu;
    const ezUInt32 uiFlat = pShader->GetFlatBinding(ezGLBindingClass::ImageUnit, static_cast<ezInt16>(uiSet), static_cast<ezInt16>(uiSlot));
    if (uiFlat == 0xFFFFFFFFu)
      continue;

    const ezGALSamplerStateHandle hSampler = it.Value();
    const ezGALSamplerStateOpenGL* pSampler = hSampler.IsInvalidated() ? nullptr : static_cast<const ezGALSamplerStateOpenGL*>(m_DeviceOpenGL.GetSamplerState(hSampler));
    glBindSampler(uiFlat, pSampler != nullptr ? pSampler->GetSampler() : 0);
  }

  // Push constants
  {
    const ezUInt32 uiFlat = pShader->GetFlatBinding(ezGLBindingClass::UniformBuffer, -1, 0);
    if (uiFlat != 0xFFFFFFFFu && m_uiPushConstantBuffer != 0)
    {
      glBindBufferBase(GL_UNIFORM_BUFFER, uiFlat, m_uiPushConstantBuffer);
    }
  }

  glActiveTexture(GL_TEXTURE0);

  m_bResourceStateDirty = false;
}

void ezGALCommandEncoderImplOpenGL::FlushBeforeRasterDraw()
{
  FlushDeferredPipelineState();
  FlushDeferredResourceState();

  if (m_uiVAO == 0)
  {
    InitializeVAO();
  }

  glBindVertexArray(m_uiVAO);
}

const ezGALTextureOpenGL* ezGALCommandEncoderImplOpenGL::ResolvePlatformTexture(const ezGALTexture* pTexture)
{
  if (pTexture == nullptr)
    return nullptr;

  return static_cast<const ezGALTextureOpenGL*>(pTexture->GetParentResource());
}

// #####################################
// ## Query functions
// #####################################

ezGALTimestampHandle ezGALCommandEncoderImplOpenGL::InsertTimestampPlatform()
{
  return m_DeviceOpenGL.GetQueryPool().InsertTimestamp();
}

ezGALOcclusionHandle ezGALCommandEncoderImplOpenGL::BeginOcclusionQueryPlatform(ezEnum<ezGALQueryType> type)
{
  return m_DeviceOpenGL.GetQueryPool().BeginOcclusionQuery(type);
}

void ezGALCommandEncoderImplOpenGL::EndOcclusionQueryPlatform(ezGALOcclusionHandle hOcclusion)
{
  m_DeviceOpenGL.GetQueryPool().EndOcclusionQuery(hOcclusion);
}

ezGALFenceHandle ezGALCommandEncoderImplOpenGL::InsertFencePlatform()
{
  GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  if (sync == nullptr)
    return 0;

  m_DeviceOpenGL.RegisterFence(sync);
  return reinterpret_cast<ezUInt64>(sync);
}

// #####################################
// ## Resource update functions
// #####################################

void ezGALCommandEncoderImplOpenGL::CopyBufferPlatform(const ezGALBuffer* pDestination, const ezGALBuffer* pSource)
{
  CopyBufferRegionPlatform(pDestination, 0, pSource, 0, ezMath::Min(pDestination->GetDescription().m_uiTotalSize, pSource->GetDescription().m_uiTotalSize));
}

void ezGALCommandEncoderImplOpenGL::CopyBufferRegionPlatform(const ezGALBuffer* pDestination, ezUInt32 uiDestOffset, const ezGALBuffer* pSource, ezUInt32 uiSourceOffset, ezUInt32 uiByteCount)
{
  const auto* pSrc = static_cast<const ezGALBufferOpenGL*>(pSource);
  const auto* pDst = static_cast<const ezGALBufferOpenGL*>(pDestination);
  glCopyNamedBufferSubData(pSrc->GetGLBuffer(), pDst->GetGLBuffer(), static_cast<GLintptr>(uiSourceOffset), static_cast<GLintptr>(uiDestOffset), static_cast<GLsizeiptr>(uiByteCount));
}

void ezGALCommandEncoderImplOpenGL::UpdateBufferPlatform(const ezGALBuffer* pDestination, ezUInt32 uiDestOffset, ezArrayPtr<const ezUInt8> sourceData, ezGALUpdateMode::Enum updateMode)
{
  const auto* pDst = static_cast<const ezGALBufferOpenGL*>(pDestination);
  const GLuint uiBuffer = pDst->GetGLBuffer();

  // Buffers created with glBufferData can be re-specified at any time. For transient buffers orphan the storage
  // so the GPU never waits on the previous contents.
  EZ_IGNORE_UNUSED(updateMode); // GL always re-specifies buffer storage implicitly via glNamedBufferSubData; transient buffers orphan below.
  if (pDst->GetDescription().m_BufferFlags.IsSet(ezGALBufferUsageFlags::Transient))
  {
    glNamedBufferData(uiBuffer, static_cast<GLsizeiptr>(pDst->GetSize()), nullptr, GL_DYNAMIC_DRAW);
  }

  glNamedBufferSubData(uiBuffer, static_cast<GLintptr>(uiDestOffset), static_cast<GLsizeiptr>(sourceData.GetCount()), sourceData.GetPtr());
}

void ezGALCommandEncoderImplOpenGL::CopyTexturePlatform(const ezGALTexture* pDestination, const ezGALTexture* pSource)
{
  const ezGALTextureOpenGL* pSrc = ResolvePlatformTexture(pSource);
  const ezGALTextureOpenGL* pDst = ResolvePlatformTexture(pDestination);

  const auto& srcDesc = pSrc->GetDescription();
  const ezUInt32 uiMipLevels = srcDesc.m_uiMipLevelCount;
  const ezUInt32 uiSliceCount = srcDesc.m_Type == ezGALTextureType::TextureCube ? srcDesc.m_uiArraySize * 6 : srcDesc.m_uiArraySize;

  for (ezUInt32 uiMip = 0; uiMip < uiMipLevels; ++uiMip)
  {
    const ezVec3U32 vSize = srcDesc.GetMipMapSize(uiMip);
    const GLsizei iDepth = srcDesc.m_Type == ezGALTextureType::Texture3D ? static_cast<GLsizei>(vSize.z) : 1;
    const ezUInt32 uiSlices = srcDesc.m_Type == ezGALTextureType::Texture3D ? 1 : uiSliceCount;
    for (ezUInt32 uiSlice = 0; uiSlice < uiSlices; ++uiSlice)
    {
      glCopyImageSubData(pSrc->GetGLTexture(), pSrc->GetTarget(), static_cast<GLint>(uiMip), 0, 0, static_cast<GLint>(uiSlice),
        pDst->GetGLTexture(), pDst->GetTarget(), static_cast<GLint>(uiMip), 0, 0, static_cast<GLint>(uiSlice), static_cast<GLsizei>(vSize.x), static_cast<GLsizei>(vSize.y), iDepth);
    }
  }
}

void ezGALCommandEncoderImplOpenGL::CopyTextureRegionPlatform(const ezGALTexture* pDestination, const ezGALTextureSubresource& destinationSubResource, const ezVec3U32& vDestinationPoint, const ezGALTexture* pSource, const ezGALTextureSubresource& sourceSubResource, const ezBoundingBoxu32& box)
{
  const ezGALTextureOpenGL* pSrc = ResolvePlatformTexture(pSource);
  const ezGALTextureOpenGL* pDst = ResolvePlatformTexture(pDestination);

  const ezVec3U32 vBoxSize = box.GetExtents();

  const GLsizei iDepth = pSrc->GetDescription().m_Type == ezGALTextureType::Texture3D ? static_cast<GLsizei>(vBoxSize.z) : 1;

  glCopyImageSubData(pSrc->GetGLTexture(), pSrc->GetTarget(), static_cast<GLint>(sourceSubResource.m_uiMipLevel), static_cast<GLint>(box.m_vMin.x), static_cast<GLint>(box.m_vMin.y), static_cast<GLint>(box.m_vMin.z + (pSrc->GetDescription().m_Type == ezGALTextureType::Texture3D ? 0 : sourceSubResource.m_uiArraySlice)),
    pDst->GetGLTexture(), pDst->GetTarget(), static_cast<GLint>(destinationSubResource.m_uiMipLevel), static_cast<GLint>(vDestinationPoint.x), static_cast<GLint>(vDestinationPoint.y), static_cast<GLint>(vDestinationPoint.z + (pDst->GetDescription().m_Type == ezGALTextureType::Texture3D ? 0 : destinationSubResource.m_uiArraySlice)),
    static_cast<GLsizei>(vBoxSize.x), static_cast<GLsizei>(vBoxSize.y), iDepth);
}

void ezGALCommandEncoderImplOpenGL::UpdateTexturePlatform(const ezGALTexture* pDestination, const ezGALTextureSubresource& destinationSubResource, const ezBoundingBoxu32& destinationBox, const ezGALSystemMemoryDescription& sourceData)
{
  const ezGALTextureOpenGL* pDst = ResolvePlatformTexture(pDestination);
  pDst->UploadSubImage(destinationSubResource, destinationBox, sourceData).IgnoreResult();
}

void ezGALCommandEncoderImplOpenGL::ResolveTexturePlatform(const ezGALTexture* pDestination, const ezGALTextureSubresource& destinationSubResource, const ezGALTexture* pSource, const ezGALTextureSubresource& sourceSubResource)
{
  const ezGALTextureOpenGL* pSrc = ResolvePlatformTexture(pSource);
  const ezGALTextureOpenGL* pDst = ResolvePlatformTexture(pDestination);

  const auto& srcDesc = pSrc->GetDescription();
  const auto& dstDesc = pDst->GetDescription();

  const bool bSrcMSAA = srcDesc.m_SampleCount != ezGALMSAASampleCount::None;
  const bool bDstMSAA = dstDesc.m_SampleCount != ezGALMSAASampleCount::None;

  if (!bSrcMSAA && !bDstMSAA)
  {
    // Plain copy, same as CopyTextureRegionPlatform with a full mip extent.
    CopyTextureRegionPlatform(pDestination, destinationSubResource, ezVec3U32(0), pSource, sourceSubResource, ezBoundingBoxu32::MakeFromMinMax(ezVec3U32(0), dstDesc.GetMipMapSize(destinationSubResource.m_uiMipLevel)));
    return;
  }

  if (!bSrcMSAA || bDstMSAA)
  {
    ezLog::Error("ResolveTexture can only resolve an MSAA source into a non-MSAA destination.");
    return;
  }

  if (pSrc->GetGLTexture() == 0 || pDst->GetGLTexture() == 0)
  {
    ezLog::Error("ResolveTexture on shared textures is not supported.");
    return;
  }

  const ezVec3U32 vSrcSize = srcDesc.GetMipMapSize(sourceSubResource.m_uiMipLevel);
  const ezVec3U32 vDstSize = dstDesc.GetMipMapSize(destinationSubResource.m_uiMipLevel);

  for (GLuint& uiFBO : m_uiResolveFBO)
  {
    if (uiFBO == 0)
      glCreateFramebuffers(1, &uiFBO);
  }

  const GLuint uiReadFBO = m_uiResolveFBO[0];
  const GLuint uiDrawFBO = m_uiResolveFBO[1];

  const GLenum eAttachment = GL_COLOR_ATTACHMENT0;

  // Attach source (read) and destination (draw)
  switch (pSrc->GetTarget())
  {
    case GL_TEXTURE_2D_MULTISAMPLE:
    case GL_TEXTURE_2D:
      glNamedFramebufferTexture(uiReadFBO, eAttachment, pSrc->GetGLTexture(), static_cast<GLint>(sourceSubResource.m_uiMipLevel));
      break;
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
    case GL_TEXTURE_2D_ARRAY:
    case GL_TEXTURE_CUBE_MAP_ARRAY:
      glNamedFramebufferTextureLayer(uiReadFBO, eAttachment, pSrc->GetGLTexture(), static_cast<GLint>(sourceSubResource.m_uiMipLevel), static_cast<GLint>(sourceSubResource.m_uiArraySlice));
      break;
    case GL_TEXTURE_CUBE_MAP:
      glNamedFramebufferTextureLayer(uiReadFBO, eAttachment, pSrc->GetGLTexture(), static_cast<GLint>(sourceSubResource.m_uiMipLevel), static_cast<GLint>(sourceSubResource.m_uiArraySlice));
      break;
    default:
      ezLog::Error("ResolveTexture: unsupported source texture type.");
      return;
  }

  switch (pDst->GetTarget())
  {
    case GL_TEXTURE_2D:
      glNamedFramebufferTexture(uiDrawFBO, eAttachment, pDst->GetGLTexture(), static_cast<GLint>(destinationSubResource.m_uiMipLevel));
      break;
    case GL_TEXTURE_2D_ARRAY:
    case GL_TEXTURE_CUBE_MAP:
    case GL_TEXTURE_CUBE_MAP_ARRAY:
      glNamedFramebufferTextureLayer(uiDrawFBO, eAttachment, pDst->GetGLTexture(), static_cast<GLint>(destinationSubResource.m_uiMipLevel), static_cast<GLint>(destinationSubResource.m_uiArraySlice));
      break;
    default:
      ezLog::Error("ResolveTexture: unsupported destination texture type.");
      return;
  }

  const GLenum eSrcTarget = GL_READ_FRAMEBUFFER;
  glNamedFramebufferReadBuffer(uiReadFBO, eAttachment);
  glNamedFramebufferDrawBuffer(uiDrawFBO, eAttachment);

  glBlitNamedFramebuffer(uiReadFBO, uiDrawFBO,
    0, 0, static_cast<GLint>(vSrcSize.x), static_cast<GLint>(vSrcSize.y),
    0, 0, static_cast<GLint>(vDstSize.x), static_cast<GLint>(vDstSize.y),
    GL_COLOR_BUFFER_BIT, GL_NEAREST);

  (void)eSrcTarget;
}

void ezGALCommandEncoderImplOpenGL::ReadbackTexturePlatform(const ezGALReadbackTexture* pDestination, const ezGALTexture* pSource)
{
  const auto* pDst = static_cast<const ezGALReadbackTextureOpenGL*>(pDestination);
  const ezGALTextureOpenGL* pSrc = ResolvePlatformTexture(pSource);

  const auto& srcDesc = pSrc->GetDescription();
  const bool bMSAASourceTexture = srcDesc.m_SampleCount != ezGALMSAASampleCount::None;
  EZ_ASSERT_DEV(!bMSAASourceTexture, "MSAA read-back not implemented!");
  if (bMSAASourceTexture)
    return;

  // Readback textures were created with the same description as the source texture (full mip chain).
  const ezUInt32 uiMipLevels = srcDesc.m_uiMipLevelCount;
  const ezUInt32 uiSliceCount = srcDesc.m_Type == ezGALTextureType::TextureCube ? srcDesc.m_uiArraySize * 6 : (srcDesc.m_Type == ezGALTextureType::Texture2DArray || srcDesc.m_Type == ezGALTextureType::TextureCubeArray || srcDesc.m_Type == ezGALTextureType::Texture2DProxy ? srcDesc.m_uiArraySize : 1);

  for (ezUInt32 uiMip = 0; uiMip < uiMipLevels; ++uiMip)
  {
    const ezVec3U32 vSize = srcDesc.GetMipMapSize(uiMip);
    const GLsizei iDepth = srcDesc.m_Type == ezGALTextureType::Texture3D ? static_cast<GLsizei>(vSize.z) : 1;
    const ezUInt32 uiSlices = srcDesc.m_Type == ezGALTextureType::Texture3D ? 1 : uiSliceCount;
    for (ezUInt32 uiSlice = 0; uiSlice < uiSlices; ++uiSlice)
    {
      glCopyImageSubData(pSrc->GetGLTexture(), pSrc->GetTarget(), static_cast<GLint>(uiMip), 0, 0, static_cast<GLint>(uiSlice),
        pDst->GetGLTexture(), pDst->GetTarget(), static_cast<GLint>(uiMip), 0, 0, static_cast<GLint>(uiSlice), static_cast<GLsizei>(vSize.x), static_cast<GLsizei>(vSize.y), iDepth);
    }
  }
}

void ezGALCommandEncoderImplOpenGL::ReadbackBufferPlatform(const ezGALReadbackBuffer* pDestination, const ezGALBuffer* pSource)
{
  const auto* pDst = static_cast<const ezGALReadbackBufferOpenGL*>(pDestination);
  const auto* pSrc = static_cast<const ezGALBufferOpenGL*>(pSource);

  EZ_ASSERT_DEV(pSource->GetSize() == pDestination->GetSize(), "Source and destination buffer sizes mismatch!");
  const ezUInt32 uiSize = pSource->GetSize();
  glCopyNamedBufferSubData(pSrc->GetGLBuffer(), pDst->GetGLBuffer(), 0, 0, static_cast<GLsizeiptr>(uiSize));
}

// #####################################
// ## Barriers
// #####################################

void ezGALCommandEncoderImplOpenGL::TextureBarrierPlatform(ezArrayPtr<const ezGALTextureBarrier> barriers)
{
  if (barriers.IsEmpty())
    return;

  // GL has no explicit resource barriers; make writes by previous passes visible and available to subsequent reads.
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void ezGALCommandEncoderImplOpenGL::BufferBarrierPlatform(ezArrayPtr<const ezGALBufferBarrier> barriers)
{
  if (barriers.IsEmpty())
    return;

  glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

// #####################################
// ## Misc
// #####################################

void ezGALCommandEncoderImplOpenGL::FlushPlatform()
{
  glFlush();
}

void ezGALCommandEncoderImplOpenGL::PushMarkerPlatform(const char* szMarker)
{
  if (glPushDebugGroup != nullptr)
  {
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, szMarker);
  }
}

void ezGALCommandEncoderImplOpenGL::PopMarkerPlatform()
{
  if (glPopDebugGroup != nullptr)
  {
    glPopDebugGroup();
  }
}

void ezGALCommandEncoderImplOpenGL::InsertEventMarkerPlatform(const char* szMarker)
{
  if (glPushDebugGroup != nullptr && glPopDebugGroup != nullptr)
  {
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, szMarker);
    glPopDebugGroup();
  }
}

// #####################################
// ## Compute dispatch
// #####################################

void ezGALCommandEncoderImplOpenGL::BeginComputePlatform()
{
  // Flush render targets so compute shaders can sample them.
  m_RenderTargetSetup = ezGALRenderingSetup();
  m_bInRendering = false;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  m_bResourceStateDirty = true;
}

void ezGALCommandEncoderImplOpenGL::EndComputePlatform()
{
  m_bResourceStateDirty = true;
}

void ezGALCommandEncoderImplOpenGL::ApplyViewportAndScissor()
{
  // With glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE) (set at device init when available), GL uses the same
  // upper-left origin as the GAL. Without it, GL is lower-left/negative-z by default and the Y axis must be flipped.
  const bool bClipControl = m_DeviceOpenGL.HasClipControl();
  const GLint iTotalHeight = static_cast<GLint>(m_RenderTargetSize.height);

  const GLint iX = static_cast<GLint>(m_Viewport.x);
  const GLint iY = static_cast<GLint>(m_Viewport.y);
  const GLsizei iWidth = static_cast<GLsizei>(m_Viewport.width);
  const GLsizei iHeight = static_cast<GLsizei>(m_Viewport.height);

  if (bClipControl)
  {
    glViewport(iX, iY, iWidth, iHeight);
  }
  else
  {
    glViewport(iX, iTotalHeight - (iY + iHeight), iWidth, iHeight);
  }
  glDepthRange(m_fMinDepth, m_fMaxDepth);

  if (m_bScissorEnabled && m_bScissorRectValid)
  {
    const GLint iScissorX = static_cast<GLint>(m_ScissorRect.x);
    const GLint iScissorY = static_cast<GLint>(m_ScissorRect.y);
    const GLsizei iScissorWidth = static_cast<GLsizei>(m_ScissorRect.width);
    const GLsizei iScissorHeight = static_cast<GLsizei>(m_ScissorRect.height);

    glEnable(GL_SCISSOR_TEST);
    if (bClipControl)
    {
      glScissor(iScissorX, iScissorY, iScissorWidth, iScissorHeight);
    }
    else
    {
      glScissor(iScissorX, iTotalHeight - (iScissorY + iScissorHeight), iScissorWidth, iScissorHeight);
    }
  }
  else
  {
    glDisable(GL_SCISSOR_TEST);
  }
}

// #####################################
// ## Dynamic state
// #####################################

void ezGALCommandEncoderImplOpenGL::SetViewportPlatform(const ezRectFloat& rect, float fMinDepth, float fMaxDepth)
{
  m_Viewport = rect;
  m_fMinDepth = fMinDepth;
  m_fMaxDepth = fMaxDepth;
  ApplyViewportAndScissor();
}

void ezGALCommandEncoderImplOpenGL::SetScissorRectPlatform(const ezRectU32& rect)
{
  m_ScissorRect = rect;
  m_bScissorRectValid = true;
  ApplyViewportAndScissor();
}

void ezGALCommandEncoderImplOpenGL::SetStencilReferencePlatform(ezUInt8 uiStencilRefValue)
{
  m_uiStencilRef = uiStencilRefValue;
  m_bStencilRefDirty = true;
}

void ezGALCommandEncoderImplOpenGL::SetIndexBufferPlatform(const ezGALBuffer* pIndexBuffer)
{
  const ezGALBufferOpenGL* pGLBuffer = static_cast<const ezGALBufferOpenGL*>(pIndexBuffer);

  m_bIndexBufferBound = pGLBuffer != nullptr;
  if (pGLBuffer != nullptr)
  {
    m_BoundIndexType = pGLBuffer->GetIndexFormat();
  }

  // The element array buffer binding is part of the VAO state.
  InitializeVAO();
  glBindVertexArray(m_uiVAO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pGLBuffer != nullptr ? pGLBuffer->GetGLBuffer() : 0);
}

void ezGALCommandEncoderImplOpenGL::SetVertexBufferPlatform(ezUInt32 uiSlot, const ezGALBuffer* pVertexBuffer, ezUInt32 uiOffset)
{
  if (uiSlot >= EZ_GAL_MAX_VERTEX_BUFFER_COUNT)
  {
    ezLog::Error("Vertex buffer slot {0} out of range.", uiSlot);
    return;
  }

  const ezGALBufferOpenGL* pGLBuffer = static_cast<const ezGALBufferOpenGL*>(pVertexBuffer);
  m_BoundVertexBuffers[uiSlot] = pGLBuffer != nullptr ? pGLBuffer->GetGLBuffer() : 0;
  m_VertexBufferOffsets[uiSlot] = uiOffset;
  m_bVertexStateDirty = true;
}

void ezGALCommandEncoderImplOpenGL::SetGraphicsPipelinePlatform(const ezGALGraphicsPipeline* pGraphicsPipeline)
{
  const ezGALGraphicsPipelineOpenGL* pGLPipeline = static_cast<const ezGALGraphicsPipelineOpenGL*>(pGraphicsPipeline);
  m_bGraphicsPipelineSet = pGLPipeline != nullptr;
  m_BoundGraphicsPipelineDesc = pGLPipeline != nullptr ? pGLPipeline->GetDescription() : ezGALGraphicsPipelineCreationDescription();
  m_bComputePipelineSet = false;

  const ezGALShaderOpenGL* pShader = pGLPipeline != nullptr
                                       ? static_cast<const ezGALShaderOpenGL*>(m_DeviceOpenGL.GetShader(pGLPipeline->GetDescription().m_hShader))
                                       : nullptr;
  glUseProgram(pShader != nullptr ? pShader->GetProgram() : 0);

  m_bPipelineStateDirty = true;
  m_bResourceStateDirty = true;
}

void ezGALCommandEncoderImplOpenGL::SetComputePipelinePlatform(const ezGALComputePipeline* pComputePipeline)
{
  const ezGALComputePipelineOpenGL* pGLPipeline = static_cast<const ezGALComputePipelineOpenGL*>(pComputePipeline);
  m_bComputePipelineSet = pGLPipeline != nullptr;
  m_BoundComputePipelineDesc = pGLPipeline != nullptr ? pGLPipeline->GetDescription() : ezGALComputePipelineCreationDescription();
  m_bGraphicsPipelineSet = false;

  const ezGALShaderOpenGL* pShader = pGLPipeline != nullptr
                                       ? static_cast<const ezGALShaderOpenGL*>(m_DeviceOpenGL.GetShader(pGLPipeline->GetDescription().m_hShader))
                                       : nullptr;
  glUseProgram(pShader != nullptr ? pShader->GetProgram() : 0);

  m_bResourceStateDirty = true;
}

void ezGALCommandEncoderImplOpenGL::ApplyStencilReference()
{
  if (!m_bGraphicsPipelineSet)
    return;

  const ezGALDepthStencilStateHandle hState = m_BoundGraphicsPipelineDesc.m_hDepthStencilState;
  if (hState.IsInvalidated())
    return;

  const auto* pState = static_cast<const ezGALDepthStencilStateOpenGL*>(m_DeviceOpenGL.GetDepthStencilState(hState));
  if (pState == nullptr)
    return;
  const auto& desc = pState->GetDescription();

  glStencilFuncSeparate(GL_FRONT, ezMappingTables::ToGLCompareFunc(desc.m_FrontFaceStencilOp.m_StencilFunc), m_uiStencilRef, desc.m_uiStencilReadMask);
  glStencilFuncSeparate(GL_BACK, ezMappingTables::ToGLCompareFunc(desc.m_BackFaceStencilOp.m_StencilFunc), m_uiStencilRef, desc.m_uiStencilReadMask);
  m_bStencilRefDirty = false;
}

// #####################################
// ## Deferred pipeline state flush
// #####################################

void ezGALCommandEncoderImplOpenGL::ApplyBlendState(const ezGALBlendStateCreationDescription& desc, ezUInt32 uiColorTargetCount)
{
  for (ezUInt32 i = 0; i < EZ_GAL_MAX_RENDERTARGET_COUNT; i++)
  {
    const ezUInt32 uiSrc = desc.m_bIndependentBlend ? i : 0;
    const ezGALRenderTargetBlendDescription& t = desc.m_RenderTargetBlendDescriptions[uiSrc];

    if (t.m_bBlendingEnabled)
    {
      glEnablei(GL_BLEND, i);
      glBlendEquationSeparatei(i, ezMappingTables::ToGLBlendOp(t.m_BlendOp), ezMappingTables::ToGLBlendOp(t.m_BlendOpAlpha));
      glBlendFuncSeparatei(i, ezMappingTables::ToGLBlendFactor(t.m_SourceBlend), ezMappingTables::ToGLBlendFactor(t.m_DestBlend), ezMappingTables::ToGLBlendFactor(t.m_SourceBlendAlpha), ezMappingTables::ToGLBlendFactor(t.m_DestBlendAlpha));
    }
    else
    {
      glDisablei(GL_BLEND, i);
    }

    glColorMaski(i,
      (t.m_uiWriteMask & 0x1) != 0 ? GL_TRUE : GL_FALSE,
      (t.m_uiWriteMask & 0x2) != 0 ? GL_TRUE : GL_FALSE,
      (t.m_uiWriteMask & 0x4) != 0 ? GL_TRUE : GL_FALSE,
      (t.m_uiWriteMask & 0x8) != 0 ? GL_TRUE : GL_FALSE);
  }

  if (desc.m_bAlphaToCoverage)
  {
    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
  }
  else
  {
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
  }

  (void)uiColorTargetCount;
}

void ezGALCommandEncoderImplOpenGL::ApplyDepthStencilState(const ezGALDepthStencilStateCreationDescription& desc)
{
  if (desc.m_bDepthEnable)
  {
    glEnable(GL_DEPTH_TEST);
  }
  else
  {
    glDisable(GL_DEPTH_TEST);
  }

  glDepthFunc(ezMappingTables::ToGLCompareFunc(desc.m_DepthTestFunc));
  glDepthMask(desc.m_bDepthWrite ? GL_TRUE : GL_FALSE);

  const ezGALStencilOpDescription& front = desc.m_FrontFaceStencilOp;
  const ezGALStencilOpDescription& back = desc.m_BackFaceStencilOp;

  if (desc.m_bStencilEnable)
  {
    glEnable(GL_STENCIL_TEST);
  }
  else
  {
    glDisable(GL_STENCIL_TEST);
  }

  glStencilOpSeparate(GL_FRONT, ezMappingTables::ToGLStencilOp(front.m_FailOp), ezMappingTables::ToGLStencilOp(front.m_DepthFailOp), ezMappingTables::ToGLStencilOp(front.m_PassOp));
  glStencilOpSeparate(GL_BACK, ezMappingTables::ToGLStencilOp(back.m_FailOp), ezMappingTables::ToGLStencilOp(back.m_DepthFailOp), ezMappingTables::ToGLStencilOp(back.m_PassOp));

  glStencilFuncSeparate(GL_FRONT, ezMappingTables::ToGLCompareFunc(front.m_StencilFunc), m_uiStencilRef, desc.m_uiStencilReadMask);
  glStencilFuncSeparate(GL_BACK, ezMappingTables::ToGLCompareFunc(back.m_StencilFunc), m_uiStencilRef, desc.m_uiStencilReadMask);

  glStencilMaskSeparate(GL_FRONT, desc.m_uiStencilWriteMask);
  glStencilMaskSeparate(GL_BACK, desc.m_uiStencilWriteMask);

  m_bStencilRefDirty = false;
}

void ezGALCommandEncoderImplOpenGL::ApplyRasterizerState(const ezGALRasterizerStateCreationDescription& desc)
{
  if (desc.m_CullMode == ezGALCullMode::None)
  {
    glDisable(GL_CULL_FACE);
  }
  else
  {
    glEnable(GL_CULL_FACE);
    glCullFace(ezMappingTables::ToGLCullMode(desc.m_CullMode));
  }

  glFrontFace(desc.m_bFrontCounterClockwise ? GL_CCW : GL_CW);
  glPolygonMode(GL_FRONT_AND_BACK, desc.m_bWireFrame ? GL_LINE : GL_FILL);

  const bool bDepthBias = desc.m_iDepthBias != 0 || desc.m_fSlopeScaledDepthBias != 0.0f || desc.m_fDepthBiasClamp != 0.0f;
  if (bDepthBias)
  {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_POLYGON_OFFSET_LINE);
    glEnable(GL_POLYGON_OFFSET_POINT);
    glPolygonOffset(desc.m_fSlopeScaledDepthBias, static_cast<GLfloat>(desc.m_iDepthBias));
  }
  else
  {
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glDisable(GL_POLYGON_OFFSET_POINT);
  }

  m_bScissorEnabled = desc.m_bScissorTest;
}

void ezGALCommandEncoderImplOpenGL::ApplyVertexState()
{
  if (!m_bVertexStateDirty)
    return;

  InitializeVAO();

  // Disable all previously enabled attributes
  for (const ezUInt32 uiLoc : m_EnabledAttributeLocations)
  {
    glDisableVertexArrayAttrib(m_uiVAO, uiLoc);
  }
  m_EnabledAttributeLocations.Clear();

  const ezGALVertexDeclarationOpenGL* pDeclaration = nullptr;
  if (m_bGraphicsPipelineSet && !m_BoundGraphicsPipelineDesc.m_hVertexDeclaration.IsInvalidated())
  {
    pDeclaration = static_cast<const ezGALVertexDeclarationOpenGL*>(m_DeviceOpenGL.GetVertexDeclaration(m_BoundGraphicsPipelineDesc.m_hVertexDeclaration));
  }

  if (pDeclaration != nullptr)
  {
    const auto& attributes = pDeclaration->GetAttributes();
    const auto& strides = pDeclaration->GetVertexBufferStrides();

    // Bind vertex buffers
    const ezUInt32 uiSlotCount = ezMath::Min<ezUInt32>(static_cast<ezUInt32>(m_BoundVertexBuffers.GetCount()), strides.GetCount());
    for (ezUInt32 uiSlot = 0; uiSlot < uiSlotCount; uiSlot++)
    {
      glVertexArrayVertexBuffer(m_uiVAO, uiSlot, m_BoundVertexBuffers[uiSlot], m_VertexBufferOffsets[uiSlot], strides[uiSlot]);
    }

    // Setup attributes
    for (const auto& attr : attributes)
    {
      const ezGALVertexBindingRate::Enum rate = attr.m_eRate;
      glVertexArrayBindingDivisor(m_uiVAO, attr.m_uiVertexBufferSlot, attr.m_uiDivisor);

      if (attr.m_bInteger)
      {
        glVertexArrayAttribIFormat(m_uiVAO, attr.m_uiLocation, attr.m_iComponents, attr.m_eType, attr.m_uiOffset);
      }
      else
      {
        glVertexArrayAttribFormat(m_uiVAO, attr.m_uiLocation, attr.m_iComponents, attr.m_eType, attr.m_bNormalized, attr.m_uiOffset);
      }

      glVertexArrayAttribBinding(m_uiVAO, attr.m_uiLocation, attr.m_uiVertexBufferSlot);
      glEnableVertexArrayAttrib(m_uiVAO, attr.m_uiLocation);
      m_EnabledAttributeLocations.PushBack(attr.m_uiLocation);
      (void)rate;
    }
  }

  m_bVertexStateDirty = false;
}

void ezGALCommandEncoderImplOpenGL::FlushDeferredPipelineState()
{
  if (!m_bGraphicsPipelineSet)
    return;

  const ezGALGraphicsPipelineCreationDescription& pipelineDesc = m_BoundGraphicsPipelineDesc;

  if (m_bPipelineStateDirty)
  {
    if (!pipelineDesc.m_hBlendState.IsInvalidated())
    {
      const auto* pState = static_cast<const ezGALBlendStateOpenGL*>(m_DeviceOpenGL.GetBlendState(pipelineDesc.m_hBlendState));
      if (pState != nullptr)
      {
        ApplyBlendState(pState->GetDescription(), m_uiColorTargetCount);
      }
    }

    if (!pipelineDesc.m_hDepthStencilState.IsInvalidated())
    {
      const auto* pState = static_cast<const ezGALDepthStencilStateOpenGL*>(m_DeviceOpenGL.GetDepthStencilState(pipelineDesc.m_hDepthStencilState));
      if (pState != nullptr)
      {
        ApplyDepthStencilState(pState->GetDescription());
      }
    }

    if (!pipelineDesc.m_hRasterizerState.IsInvalidated())
    {
      const auto* pState = static_cast<const ezGALRasterizerStateOpenGL*>(m_DeviceOpenGL.GetRasterizerState(pipelineDesc.m_hRasterizerState));
      if (pState != nullptr)
      {
        ApplyRasterizerState(pState->GetDescription());
      }
    }

    m_bPipelineStateDirty = false;
  }
  else if (m_bStencilRefDirty)
  {
    ApplyStencilReference();
  }

  ApplyVertexState();
  ApplyViewportAndScissor();
}

constexpr GLenum ToGLDrawBuffers(ezUInt32 i)
{
  return GL_COLOR_ATTACHMENT0 + i;
}

void ezGALCommandEncoderImplOpenGL::SetDrawBuffersMask(ezUInt32 uiMask)
{
  if (uiMask == m_uiCurrentDrawBuffersMask && m_uiCurrentFBO != 0)
    return;

  GLenum eBuffers[EZ_GAL_MAX_RENDERTARGET_COUNT];
  ezUInt32 uiCount = 0;
  for (ezUInt32 i = 0; i < EZ_GAL_MAX_RENDERTARGET_COUNT; i++)
  {
    if (uiMask & (1u << i))
    {
      eBuffers[uiCount++] = ToGLDrawBuffers(i);
    }
  }

  glDrawBuffers(static_cast<GLsizei>(uiCount), uiCount > 0 ? eBuffers : nullptr);
  m_uiCurrentDrawBuffersMask = uiMask;
}

GLenum ezGALCommandEncoderImplOpenGL::GetPrimitiveMode() const
{
  if (!m_bGraphicsPipelineSet)
    return GL_TRIANGLES;

  return ezMappingTables::ToGLPrimitiveTopology(m_BoundGraphicsPipelineDesc.m_Topology);
}

// #####################################
// ## Frame buffer object management
// #####################################

void ezGALCommandEncoderImplOpenGL::AttachTexture(GLuint uiFBO, GLenum eAttachment, const ezGALRenderTargetView* pView)
{
  if (pView == nullptr)
    return;

  const ezGALTextureOpenGL* pTexture = ResolvePlatformTexture(pView->GetTexture());
  if (pTexture == nullptr)
    return;

  const GLuint uiTexture = pTexture->GetGLTexture();
  if (uiTexture == 0)
  {
    ezLog::Error("Render target view references a texture without a GL texture object.");
    return;
  }

  const auto& viewDesc = pView->GetDescription();
  const auto& texDesc = pTexture->GetDescription();
  const GLint iLevel = static_cast<GLint>(viewDesc.m_uiMipLevel);
  const GLint iLayer = static_cast<GLint>(viewDesc.m_uiFirstSlice);

  switch (pTexture->GetTarget())
  {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_2D_MULTISAMPLE:
      glNamedFramebufferTexture(uiFBO, eAttachment, uiTexture, iLevel);
      break;

    case GL_TEXTURE_2D_ARRAY:
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
    case GL_TEXTURE_CUBE_MAP:
    case GL_TEXTURE_CUBE_MAP_ARRAY:
      glNamedFramebufferTextureLayer(uiFBO, eAttachment, uiTexture, iLevel, iLayer);
      break;

    case GL_TEXTURE_3D:
    {
      // glNamedFramebufferTexture3D does not exist; bind temporarily.
      GLint iPrevFBO = 0;
      glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &iPrevFBO);
      glBindFramebuffer(GL_FRAMEBUFFER, uiFBO);
      glFramebufferTexture3D(GL_FRAMEBUFFER, eAttachment, GL_TEXTURE_3D, uiTexture, iLevel, iLayer);
      glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(iPrevFBO));
      break;
    }

    default:
      ezLog::Error("Unsupported texture target {0} for render target attachment.", static_cast<ezUInt32>(pTexture->GetTarget()));
      break;
  }

  (void)texDesc;
}

GLuint ezGALCommandEncoderImplOpenGL::GetOrCreateFBO(const ezGALFrameBufferDescriptor& desc, ezUInt32 uiColorTargetCount)
{
  for (const auto& entry : m_FBOCache)
  {
    if (entry.m_Desc == desc)
      return entry.m_uiFBO;
  }

  GLuint uiFBO = 0;
  glCreateFramebuffers(1, &uiFBO);
  if (uiFBO == 0)
    return 0;

  ezUInt32 uiDrawMask = 0;

  for (ezUInt32 i = 0; i < uiColorTargetCount; i++)
  {
    if (desc.m_hColorTarget[i].IsInvalidated())
      continue;

    const ezGALRenderTargetView* pView = m_DeviceOpenGL.GetRenderTargetView(desc.m_hColorTarget[i]);
    AttachTexture(uiFBO, GL_COLOR_ATTACHMENT0 + i, pView);
    uiDrawMask |= (1u << i);
  }

  if (!desc.m_hDepthTarget.IsInvalidated())
  {
    const ezGALRenderTargetView* pDepthView = m_DeviceOpenGL.GetRenderTargetView(desc.m_hDepthTarget);
    const ezGALTexture* pDepthTexture = pDepthView != nullptr ? static_cast<const ezGALTexture*>(pDepthView->GetTexture()->GetParentResource()) : nullptr;
    const ezEnum<ezGALResourceFormat> eFormat = pDepthTexture != nullptr ? pDepthTexture->GetDescription().m_Format : ezEnum<ezGALResourceFormat>(ezGALResourceFormat::Invalid);
    const bool bHasStencil = ezGALResourceFormat::IsStencilFormat(eFormat);
    AttachTexture(uiFBO, bHasStencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, pDepthView);
  }

  // Setup draw buffers
  GLenum eDrawBuffers[EZ_GAL_MAX_RENDERTARGET_COUNT];
  ezUInt32 uiCount = 0;
  for (ezUInt32 i = 0; i < uiColorTargetCount; i++)
  {
    if (uiDrawMask & (1u << i))
    {
      eDrawBuffers[uiCount++] = GL_COLOR_ATTACHMENT0 + i;
    }
  }

  glNamedFramebufferDrawBuffers(uiFBO, static_cast<GLsizei>(uiCount), uiCount > 0 ? eDrawBuffers : nullptr);

  if (uiCount == 0)
  {
    glNamedFramebufferDrawBuffer(uiFBO, GL_NONE);
  }

  const GLenum eStatus = glCheckNamedFramebufferStatus(uiFBO, GL_FRAMEBUFFER);
  if (eStatus != GL_FRAMEBUFFER_COMPLETE)
  {
    ezLog::Error("Framebuffer object is incomplete (status {0}).", static_cast<ezUInt32>(eStatus));
    glDeleteFramebuffers(1, &uiFBO);
    return 0;
  }

  FBOEntry entry;
  entry.m_Desc = desc;
  entry.m_uiFBO = uiFBO;
  entry.m_uiDrawBuffersMask = uiDrawMask;
  m_FBOCache.PushBack(entry);
  return uiFBO;
}

ezUInt32 ezGALCommandEncoderImplOpenGL::BindFBO(const ezGALFrameBufferDescriptor& desc, ezUInt32 uiColorTargetCount)
{
  const GLuint uiFBO = GetOrCreateFBO(desc, uiColorTargetCount);
  if (uiFBO == 0)
    return 0;

  glBindFramebuffer(GL_FRAMEBUFFER, uiFBO);
  m_uiCurrentFBO = uiFBO;

  for (const auto& entry : m_FBOCache)
  {
    if (entry.m_uiFBO == uiFBO)
    {
      m_uiCurrentDrawBuffersMask = entry.m_uiDrawBuffersMask;
      break;
    }
  }

  return m_uiCurrentDrawBuffersMask;
}


// #####################################
// ## Rendering
// #####################################

void ezGALCommandEncoderImplOpenGL::BeginRenderingPlatform(const ezGALRenderingSetup& renderingSetup)
{
  m_RenderTargetSetup = renderingSetup;
  m_bInRendering = true;

  const ezGALFrameBufferDescriptor& fbDesc = renderingSetup.GetFrameBuffer();
  m_FrameBuffer = fbDesc;
  m_RenderTargetSize = fbDesc.m_Size;

  const ezUInt32 uiColorTargetCount = renderingSetup.GetColorTargetCount();
  m_uiColorTargetCount = uiColorTargetCount;

  const ezGALRenderPassDescriptor& renderPass = renderingSetup.GetRenderPass();
  m_bMultisampled = renderPass.m_Msaa != ezGALMSAASampleCount::None;

  // Determine depth read-only status
  m_bDepthReadOnly = false;
  if (renderingSetup.HasDepthStencilTarget() && !fbDesc.m_hDepthTarget.IsInvalidated())
  {
    const ezGALRenderTargetView* pDepthView = m_DeviceOpenGL.GetRenderTargetView(fbDesc.m_hDepthTarget);
    if (pDepthView != nullptr)
    {
      m_bDepthReadOnly = pDepthView->GetDescription().m_bReadOnly;
    }
  }

  const ezUInt32 uiDrawMask = BindFBO(fbDesc, uiColorTargetCount);

  // Always set the scissor rect to the render target; the engine will refine it if needed.
  m_ScissorRect = ezRectU32(0, 0, fbDesc.m_Size.width, fbDesc.m_Size.height);
  m_bScissorRectValid = true;
  ApplyViewportAndScissor();

  // Handle load ops that clear
  ezUInt32 uiColorClearMask = 0;
  for (ezUInt32 i = 0; i < uiColorTargetCount; i++)
  {
    if (renderPass.m_ColorLoadOp[i] == ezGALRenderTargetLoadOp::Clear)
    {
      uiColorClearMask |= (1u << i);
    }
  }

  const bool bClearDepth = renderPass.m_DepthLoadOp == ezGALRenderTargetLoadOp::Clear && renderingSetup.HasDepthStencilTarget();
  const bool bClearStencil = renderPass.m_StencilLoadOp == ezGALRenderTargetLoadOp::Clear && renderingSetup.HasDepthStencilTarget();

  if (uiColorClearMask != 0)
  {
    const GLboolean bAlphaMask[EZ_GAL_MAX_RENDERTARGET_COUNT] = {};
    (void)bAlphaMask;

    // Make sure color writes are enabled for clearing.
    for (ezUInt32 i = 0; i < uiColorTargetCount; i++)
    {
      glColorMaski(i, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    for (ezUInt32 i = 0; i < uiColorTargetCount; i++)
    {
      if ((uiColorClearMask & (1u << i)) == 0)
        continue;

      const GLenum eBuf = GL_COLOR_ATTACHMENT0 + i;
      glDrawBuffers(1, &eBuf);
      const ezColor col = renderingSetup.GetClearColor(static_cast<ezUInt8>(i));
      glClearColor(col.r, col.g, col.b, col.a);
      glClear(GL_COLOR_BUFFER_BIT);
    }

    // Restore draw buffers
    GLenum eBufs[EZ_GAL_MAX_RENDERTARGET_COUNT];
    GLsizei iCount = 0;
    for (ezUInt32 i = 0; i < uiColorTargetCount; i++)
    {
      if (uiDrawMask & (1u << i))
      {
        eBufs[iCount++] = GL_COLOR_ATTACHMENT0 + i;
      }
    }
    if (iCount > 0)
    {
      glDrawBuffers(iCount, eBufs);
    }
    else
    {
      glDrawBuffers(0, nullptr);
    }

    // Re-apply per-buffer color masks from the blend state.
    m_bPipelineStateDirty = true;
  }

  if (bClearDepth || bClearStencil)
  {
    glDepthMask(GL_TRUE);
    glStencilMask(0xFF);

    glClearDepth(renderingSetup.GetClearDepth());
    glClearStencil(renderingSetup.GetClearStencil());

    GLbitfield eBits = 0;
    if (bClearDepth)
      eBits |= GL_DEPTH_BUFFER_BIT;
    if (bClearStencil)
      eBits |= GL_STENCIL_BUFFER_BIT;
    glClear(eBits);

    // Re-apply depth / stencil masks from the depth-stencil state.
    m_bPipelineStateDirty = true;
  }

  // Handle DontCare load ops by invalidating the attachments
  GLenum eDiscard[EZ_GAL_MAX_RENDERTARGET_COUNT + 2];
  GLsizei iDiscardCount = 0;
  for (ezUInt32 i = 0; i < uiColorTargetCount; i++)
  {
    if (renderPass.m_ColorLoadOp[i] == ezGALRenderTargetLoadOp::DontCare)
    {
      eDiscard[iDiscardCount++] = GL_COLOR_ATTACHMENT0 + i;
    }
  }
  if (renderingSetup.HasDepthStencilTarget())
  {
    if (renderPass.m_DepthLoadOp == ezGALRenderTargetLoadOp::DontCare && renderPass.m_StencilLoadOp == ezGALRenderTargetLoadOp::DontCare)
    {
      eDiscard[iDiscardCount++] = GL_DEPTH_STENCIL_ATTACHMENT;
    }
    else if (renderPass.m_DepthLoadOp == ezGALRenderTargetLoadOp::DontCare)
    {
      eDiscard[iDiscardCount++] = GL_DEPTH_ATTACHMENT;
    }
    else if (renderPass.m_StencilLoadOp == ezGALRenderTargetLoadOp::DontCare)
    {
      eDiscard[iDiscardCount++] = GL_STENCIL_ATTACHMENT;
    }
  }
  if (iDiscardCount > 0 && glInvalidateFramebuffer != nullptr)
  {
    glInvalidateFramebuffer(GL_FRAMEBUFFER, iDiscardCount, eDiscard);
  }
}

void ezGALCommandEncoderImplOpenGL::EndRenderingPlatform()
{
  m_bInRendering = false;
  m_RenderTargetSetup = ezGALRenderingSetup();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  m_uiCurrentFBO = 0;
  m_uiCurrentDrawBuffersMask = 0;

  // Unbind push constant / storage bindings that may alias the resolve target? No, nothing to do.
}

void ezGALCommandEncoderImplOpenGL::ClearPlatform(const ezColor& clearColor, ezUInt32 uiRenderTargetClearMask, bool bClearDepth, bool bClearStencil, float fDepthClear, ezUInt8 uiStencilClear)
{
  if (uiRenderTargetClearMask != 0 && m_uiColorTargetCount > 0)
  {
    const GLint iPrevDrawFBO = []() -> GLint
    {
      GLint i = 0;
      glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &i);
      return i;
    }();
    (void)iPrevDrawFBO;

    for (ezUInt32 i = 0; i < m_uiColorTargetCount; i++)
    {
      glColorMaski(i, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    for (ezUInt32 i = 0; i < m_uiColorTargetCount; i++)
    {
      if ((uiRenderTargetClearMask & (1u << i)) == 0)
        continue;

      const GLenum eBuf = GL_COLOR_ATTACHMENT0 + i;
      glDrawBuffers(1, &eBuf);
      glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
      glClear(GL_COLOR_BUFFER_BIT);
    }

    GLenum eBufs[EZ_GAL_MAX_RENDERTARGET_COUNT];
    GLsizei iCount = 0;
    for (ezUInt32 i = 0; i < m_uiColorTargetCount; i++)
    {
      if (m_uiCurrentDrawBuffersMask & (1u << i))
      {
        eBufs[iCount++] = GL_COLOR_ATTACHMENT0 + i;
      }
    }
    if (iCount > 0)
    {
      glDrawBuffers(iCount, eBufs);
    }
    else
    {
      glDrawBuffers(0, nullptr);
    }

    m_bPipelineStateDirty = true; // restore color masks
  }

  if (bClearDepth || bClearStencil)
  {
    glDepthMask(GL_TRUE);
    glStencilMask(0xFF);

    glClearDepth(fDepthClear);
    glClearStencil(uiStencilClear);

    GLbitfield eBits = 0;
    if (bClearDepth)
      eBits |= GL_DEPTH_BUFFER_BIT;
    if (bClearStencil)
      eBits |= GL_STENCIL_BUFFER_BIT;
    glClear(eBits);

    m_bPipelineStateDirty = true; // restore depth/stencil masks
  }
}

// #####################################
// ## Draw & dispatch
// #####################################

ezResult ezGALCommandEncoderImplOpenGL::DrawPlatform(ezUInt32 uiVertexCount, ezUInt32 uiStartVertex)
{
  FlushBeforeRasterDraw();
  glDrawArrays(GetPrimitiveMode(), static_cast<GLint>(uiStartVertex), static_cast<GLsizei>(uiVertexCount));
  return EZ_SUCCESS;
}

ezResult ezGALCommandEncoderImplOpenGL::DrawIndexedPlatform(ezUInt32 uiIndexCount, ezUInt32 uiStartIndex)
{
  FlushBeforeRasterDraw();

  if (!m_bIndexBufferBound)
  {
    ezLog::Error("DrawIndexed called without a bound index buffer.");
    return EZ_FAILURE;
  }

  const ezGALIndexType::Enum eIndexType = m_BoundIndexType;
  const GLenum eGLIndexType = eIndexType == ezGALIndexType::UShort ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
  const ezUInt32 uiIndexSize = ezGALIndexType::GetSize(eIndexType);

  const void* pIndices = reinterpret_cast<const void*>(static_cast<uintptr_t>(uiStartIndex) * uiIndexSize);
  glDrawElements(GetPrimitiveMode(), static_cast<GLsizei>(uiIndexCount), eGLIndexType, pIndices);
  return EZ_SUCCESS;
}

ezResult ezGALCommandEncoderImplOpenGL::DrawIndexedInstancedPlatform(ezUInt32 uiIndexCountPerInstance, ezUInt32 uiInstanceCount, ezUInt32 uiStartIndex)
{
  FlushBeforeRasterDraw();

  if (!m_bIndexBufferBound)
  {
    ezLog::Error("DrawIndexedInstanced called without a bound index buffer.");
    return EZ_FAILURE;
  }

  const ezGALIndexType::Enum eIndexType = m_BoundIndexType;
  const GLenum eGLIndexType = eIndexType == ezGALIndexType::UShort ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
  const ezUInt32 uiIndexSize = ezGALIndexType::GetSize(eIndexType);

  const void* pIndices = reinterpret_cast<const void*>(static_cast<uintptr_t>(uiStartIndex) * uiIndexSize);
  glDrawElementsInstanced(GetPrimitiveMode(), static_cast<GLsizei>(uiIndexCountPerInstance), eGLIndexType, pIndices, static_cast<GLsizei>(uiInstanceCount));
  return EZ_SUCCESS;
}

ezResult ezGALCommandEncoderImplOpenGL::DrawIndexedInstancedIndirectPlatform(const ezGALBuffer* pIndirectArgumentBuffer, ezUInt32 uiArgumentOffsetInBytes)
{
  FlushBeforeRasterDraw();

  if (!m_bIndexBufferBound)
  {
    ezLog::Error("DrawIndexedInstancedIndirect called without a bound index buffer.");
    return EZ_FAILURE;
  }

  const auto* pIndirect = static_cast<const ezGALBufferOpenGL*>(pIndirectArgumentBuffer);
  const ezGALIndexType::Enum eIndexType = m_BoundIndexType;
  const GLenum eGLIndexType = eIndexType == ezGALIndexType::UShort ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;

  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, pIndirect->GetGLBuffer());
  glDrawElementsIndirect(GetPrimitiveMode(), eGLIndexType, reinterpret_cast<const void*>(static_cast<uintptr_t>(uiArgumentOffsetInBytes)));
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
  return EZ_SUCCESS;
}

ezResult ezGALCommandEncoderImplOpenGL::DrawInstancedPlatform(ezUInt32 uiVertexCountPerInstance, ezUInt32 uiInstanceCount, ezUInt32 uiStartVertex)
{
  FlushBeforeRasterDraw();
  glDrawArraysInstanced(GetPrimitiveMode(), static_cast<GLint>(uiStartVertex), static_cast<GLsizei>(uiVertexCountPerInstance), static_cast<GLsizei>(uiInstanceCount));
  return EZ_SUCCESS;
}

ezResult ezGALCommandEncoderImplOpenGL::DrawInstancedIndirectPlatform(const ezGALBuffer* pIndirectArgumentBuffer, ezUInt32 uiArgumentOffsetInBytes)
{
  FlushBeforeRasterDraw();

  const auto* pIndirect = static_cast<const ezGALBufferOpenGL*>(pIndirectArgumentBuffer);
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, pIndirect->GetGLBuffer());
  glDrawArraysIndirect(GetPrimitiveMode(), reinterpret_cast<const void*>(static_cast<uintptr_t>(uiArgumentOffsetInBytes)));
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
  return EZ_SUCCESS;
}

ezResult ezGALCommandEncoderImplOpenGL::DispatchPlatform(ezUInt32 uiThreadGroupCountX, ezUInt32 uiThreadGroupCountY, ezUInt32 uiThreadGroupCountZ)
{
  FlushDeferredResourceState();
  glDispatchCompute(uiThreadGroupCountX, uiThreadGroupCountY, uiThreadGroupCountZ);
  return EZ_SUCCESS;
}

ezResult ezGALCommandEncoderImplOpenGL::DispatchIndirectPlatform(const ezGALBuffer* pIndirectArgumentBuffer, ezUInt32 uiArgumentOffsetInBytes)
{
  FlushDeferredResourceState();

  const auto* pIndirect = static_cast<const ezGALBufferOpenGL*>(pIndirectArgumentBuffer);
  glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, pIndirect->GetGLBuffer());
  glDispatchComputeIndirect(static_cast<GLintptr>(uiArgumentOffsetInBytes));
  glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
  return EZ_SUCCESS;
}
