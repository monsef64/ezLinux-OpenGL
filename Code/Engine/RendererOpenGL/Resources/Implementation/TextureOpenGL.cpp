#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <RendererFoundation/Resources/ResourceFormats.h>
#include <RendererOpenGL/Resources/TextureOpenGL.h>
#include <RendererOpenGL/Utils/MappingTablesOpenGL.h>

#include <Foundation/Types/ScopeExit.h>

#include <cstring>

// GL has no way to describe a row pitch for compressed texture uploads, so padded rows are repacked to tight rows here.
static void RepackCompressedToTight(const ezGALSystemMemoryDescription& sourceData, ezUInt32 uiFullWidth, ezUInt32 uiFullHeight, ezUInt32 uiDepth, ezUInt32 uiBlockBytes, ezDynamicArray<ezUInt8>& out_storage)
{
  const ezUInt32 uiBlocksPerRow = (uiFullWidth + 3) / 4;
  const ezUInt32 uiBlockRows = (uiFullHeight + 3) / 4;
  const ezUInt32 uiTightRowPitch = uiBlocksPerRow * uiBlockBytes;
  const ezUInt32 uiTightSlicePitch = uiTightRowPitch * uiBlockRows;

  const ezUInt32 uiSourceRowPitch = sourceData.m_uiRowPitch;
  const ezUInt32 uiSourceSlicePitch = sourceData.m_uiSlicePitch != 0 ? sourceData.m_uiSlicePitch : uiSourceRowPitch * uiBlockRows;

  out_storage.SetCountUninitialized(uiTightSlicePitch * uiDepth);
  const ezUInt8* pSrc = static_cast<const ezUInt8*>(sourceData.m_pData.GetPtr());
  for (ezUInt32 z = 0; z < uiDepth; ++z)
  {
    for (ezUInt32 y = 0; y < uiBlockRows; ++y)
    {
      memcpy(out_storage.GetData() + z * uiTightSlicePitch + y * uiTightRowPitch, pSrc + static_cast<size_t>(z) * uiSourceSlicePitch + static_cast<size_t>(y) * uiSourceRowPitch, uiTightRowPitch);
    }
  }
}

ezGALTextureOpenGL::ezGALTextureOpenGL(const ezGALTextureCreationDescription& Description)
  : ezGALTexture(Description)
{
}

ezGALTextureOpenGL::~ezGALTextureOpenGL() = default;

GLenum ezGALTextureOpenGL::ToGLTarget(ezGALTextureType::Enum type, bool bMSAA)
{
  switch (type)
  {
    case ezGALTextureType::Texture2D:
    case ezGALTextureType::Texture2DProxy:
    case ezGALTextureType::Texture2DShared:
      return bMSAA ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
    case ezGALTextureType::Texture2DArray:
      return bMSAA ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY;
    case ezGALTextureType::Texture3D:
      return GL_TEXTURE_3D;
    case ezGALTextureType::TextureCube:
      return GL_TEXTURE_CUBE_MAP;
    case ezGALTextureType::TextureCubeArray:
      return GL_TEXTURE_CUBE_MAP_ARRAY;
    default:
      return bMSAA ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
  }
}

ezResult ezGALTextureOpenGL::InitPlatform(ezGALDevice* pDevice, ezArrayPtr<ezGALSystemMemoryDescription> pInitialData)
{
  EZ_IGNORE_UNUSED(pDevice);

  m_bMSAA = m_Description.m_SampleCount != ezGALMSAASampleCount::None;
  m_eTarget = ToGLTarget(m_Description.m_Type, m_bMSAA);
  m_uiMipLevels = m_Description.m_uiMipLevelCount;
  m_uiArraySize = m_Description.m_Type == ezGALTextureType::TextureCube ? 6 : m_Description.m_Type == ezGALTextureType::TextureCubeArray ? m_Description.m_uiArraySize * 6 : m_Description.m_uiArraySize;

  const ezUInt32 uiExpectedInitialDataCount = m_uiMipLevels * m_uiArraySize;

  if (!pInitialData.IsEmpty() && pInitialData.GetCount() != uiExpectedInitialDataCount)
  {
    ezLog::Error("Texture initial data count ({0}) does not match the expected number of sub-resources ({1}).", pInitialData.GetCount(), uiExpectedInitialDataCount);
    return EZ_FAILURE;
  }

  // Create the texture object
  if (glCreateTextures != nullptr)
  {
    glCreateTextures(m_eTarget, 1, &m_uiTexture);
  }
  else
  {
    glGenTextures(1, &m_uiTexture);
    glBindTexture(m_eTarget, m_uiTexture);
  }

  if (m_uiTexture == 0)
  {
    ezLog::Error("Failed to create GL texture");
    return EZ_FAILURE;
  }

  if (CreateTextureStorage().Failed())
  {
    glDeleteTextures(1, &m_uiTexture);
    m_uiTexture = 0;
    return EZ_FAILURE;
  }

  if (!m_bMSAA && !pInitialData.IsEmpty())
  {
    if (UploadInitialData(pInitialData).Failed())
    {
      glDeleteTextures(1, &m_uiTexture);
      m_uiTexture = 0;
      return EZ_FAILURE;
    }
  }

  return EZ_SUCCESS;
}

ezResult ezGALTextureOpenGL::CreateTextureStorage()
{
  const GLenum eInternalFormat = ezMappingTables::ToGLTextureInternalFormat(m_Description.m_Format);
  if (eInternalFormat == 0)
  {
    ezLog::Error("Texture format {0} is not supported by the OpenGL backend.", m_Description.m_Format.GetValue());
    return EZ_FAILURE;
  }

  const GLsizei uiWidth = static_cast<GLsizei>(m_Description.m_uiWidth);
  const GLsizei uiHeight = static_cast<GLsizei>(m_Description.m_uiHeight);
  const GLsizei uiDepth = static_cast<GLsizei>(m_Description.m_uiDepth);
  const GLsizei uiArraySize = static_cast<GLsizei>(m_uiArraySize);
  const GLsizei uiLevels = static_cast<GLsizei>(m_uiMipLevels);

  const GLsizei uiSampleCount = [](ezGALMSAASampleCount::Enum sc) -> GLsizei {
    switch (sc)
    {
      case ezGALMSAASampleCount::None: return 1;
      case ezGALMSAASampleCount::TwoSamples: return 2;
      case ezGALMSAASampleCount::FourSamples: return 4;
      case ezGALMSAASampleCount::EightSamples: return 8;
      default: return 1;
    }
  }(m_Description.m_SampleCount);

  if (glTextureStorage2D != nullptr)
  {
    switch (m_eTarget)
    {
      case GL_TEXTURE_2D:
        glTextureStorage2D(m_uiTexture, uiLevels, eInternalFormat, uiWidth, uiHeight);
        break;
      case GL_TEXTURE_2D_MULTISAMPLE:
        glTextureStorage2DMultisample(m_uiTexture, uiSampleCount, eInternalFormat, uiWidth, uiHeight, GL_TRUE);
        break;
      case GL_TEXTURE_2D_ARRAY:
        glTextureStorage3D(m_uiTexture, uiLevels, eInternalFormat, uiWidth, uiHeight, uiArraySize);
        break;
      case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        if (glTextureStorage3DMultisample != nullptr)
        {
          glTextureStorage3DMultisample(m_uiTexture, uiSampleCount, eInternalFormat, uiWidth, uiHeight, uiArraySize, GL_TRUE);
        }
        else if (glTexImage3DMultisample != nullptr)
        {
          glBindTexture(m_eTarget, m_uiTexture);
          glTexImage3DMultisample(m_eTarget, uiSampleCount, eInternalFormat, uiWidth, uiHeight, uiArraySize, GL_TRUE);
          glBindTexture(m_eTarget, 0);
        }
        else
        {
          ezLog::Error("MSAA texture arrays are not supported by this OpenGL implementation.");
          return EZ_FAILURE;
        }
        break;
      case GL_TEXTURE_3D:
        glTextureStorage3D(m_uiTexture, uiLevels, eInternalFormat, uiWidth, uiHeight, uiDepth);
        break;
      case GL_TEXTURE_CUBE_MAP:
        glTextureStorage2D(m_uiTexture, uiLevels, eInternalFormat, uiWidth, uiHeight);
        break;
      case GL_TEXTURE_CUBE_MAP_ARRAY:
        glTextureStorage3D(m_uiTexture, uiLevels, eInternalFormat, uiWidth, uiHeight, uiArraySize);
        break;
      default:
        ezLog::Error("Unsupported GL texture target {0}.", m_eTarget);
        return EZ_FAILURE;
    }
  }
  else
  {
    glBindTexture(m_eTarget, m_uiTexture);

    const GLenum eFormatUpload = ezMappingTables::ToGLTextureFormat(m_Description.m_Format) == 0 ? GL_RGBA : ezMappingTables::ToGLTextureFormat(m_Description.m_Format);
    const GLenum eTypeUpload = ezMappingTables::ToGLTextureType(m_Description.m_Format) == 0 ? GL_UNSIGNED_BYTE : ezMappingTables::ToGLTextureType(m_Description.m_Format);

    const auto allocateLevel = [&](GLsizei level) {
      const GLsizei uiW = ezMath::Max<GLsizei>(1, uiWidth >> level);
      const GLsizei uiH = ezMath::Max<GLsizei>(1, uiHeight >> level);
      const GLsizei uiD = ezMath::Max<GLsizei>(1, uiDepth >> level);

      switch (m_eTarget)
      {
        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_CUBE_MAP_ARRAY:
          glTexImage3D(m_eTarget, level, static_cast<GLint>(eInternalFormat), uiW, uiH, m_eTarget == GL_TEXTURE_3D ? uiD : uiArraySize, 0, eFormatUpload, eTypeUpload, nullptr);
          break;
        case GL_TEXTURE_CUBE_MAP:
          for (GLsizei face = 0; face < 6; face++)
          {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, static_cast<GLint>(eInternalFormat), uiW, uiH, 0, eFormatUpload, eTypeUpload, nullptr);
          }
          break;
        default:
          glTexImage2D(m_eTarget, level, static_cast<GLint>(eInternalFormat), uiW, uiH, 0, eFormatUpload, eTypeUpload, nullptr);
          break;
      }
    };

    if (m_bMSAA)
    {
      if (m_eTarget == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
      {
        if (glTexImage3DMultisample != nullptr)
        {
          glTexImage3DMultisample(m_eTarget, uiSampleCount, eInternalFormat, uiWidth, uiHeight, uiArraySize, GL_TRUE);
        }
        else
        {
          ezLog::Error("MSAA texture arrays are not supported by this OpenGL implementation.");
          glBindTexture(m_eTarget, 0);
          return EZ_FAILURE;
        }
      }
      else
      {
        glTexImage2DMultisample(m_eTarget, uiSampleCount, eInternalFormat, uiWidth, uiHeight, GL_TRUE);
      }
    }
    else
    {
      for (GLsizei level = 0; level < uiLevels; level++)
      {
        allocateLevel(level);
      }
    }

    glBindTexture(m_eTarget, 0);
  }

  // Default texture parameters
  if (glTextureParameteri != nullptr)
  {
    if (!m_bMSAA)
    {
      glTextureParameteri(m_uiTexture, GL_TEXTURE_MAX_LEVEL, uiLevels - 1);
      glTextureParameteri(m_uiTexture, GL_TEXTURE_BASE_LEVEL, 0);
      glTextureParameteri(m_uiTexture, GL_TEXTURE_MIN_FILTER, uiLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
      glTextureParameteri(m_uiTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTextureParameteri(m_uiTexture, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTextureParameteri(m_uiTexture, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTextureParameteri(m_uiTexture, GL_TEXTURE_WRAP_R, GL_REPEAT);
    }
  }
  else
  {
    glBindTexture(m_eTarget, m_uiTexture);
    if (!m_bMSAA)
    {
      glTexParameteri(m_eTarget, GL_TEXTURE_MAX_LEVEL, uiLevels - 1);
      glTexParameteri(m_eTarget, GL_TEXTURE_BASE_LEVEL, 0);
      glTexParameteri(m_eTarget, GL_TEXTURE_MIN_FILTER, uiLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
      glTexParameteri(m_eTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(m_eTarget, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(m_eTarget, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexParameteri(m_eTarget, GL_TEXTURE_WRAP_R, GL_REPEAT);
    }
    glBindTexture(m_eTarget, 0);
  }

  return EZ_SUCCESS;
}

ezResult ezGALTextureOpenGL::UploadInitialData(ezArrayPtr<ezGALSystemMemoryDescription> pInitialData)
{
  const ezUInt32 uiMipLevels = m_uiMipLevels;
  const ezUInt32 uiArraySize = m_uiArraySize;

  const GLenum eFormat = ezMappingTables::ToGLTextureFormat(m_Description.m_Format);
  const GLenum eType = ezMappingTables::ToGLTextureType(m_Description.m_Format);
  const bool bCompressed = ezGALResourceFormat::IsBlockCompressed(m_Description.m_Format);

  ezUInt32 uiBlockBytes = 0;
  ezMappingTables::IsBlockCompressedFormat(m_Description.m_Format, uiBlockBytes);

  const GLenum eTarget = m_eTarget;
  // Cube maps are uploaded face-by-face using the face targets.
  const auto FaceTarget = [](GLenum target, ezUInt32 uiFace) -> GLenum { return target == GL_TEXTURE_CUBE_MAP ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(uiFace) : target; };

  const auto uploadSlice = [&](ezUInt32 uiSlice, ezUInt32 uiMipLevel) -> ezResult {
    const ezGALSystemMemoryDescription& desc = pInitialData[uiMipLevel + uiSlice * uiMipLevels];

    // Bind the texture so the classic (non-DSA) fallback entry points operate on it.
    // DSA entry points ignore the binding. For cube maps this binds all six faces.
    glBindTexture(eTarget, m_uiTexture);
    auto cleanupBind = ezMakeScopeExit([&] { glBindTexture(eTarget, 0); });

    const ezUInt32 uiWidth = ezMath::Max<ezUInt32>(1, m_Description.m_uiWidth >> uiMipLevel);
    const ezUInt32 uiHeight = ezMath::Max<ezUInt32>(1, m_Description.m_uiHeight >> uiMipLevel);
    const ezUInt32 uiDepth = ezMath::Max<ezUInt32>(1, m_Description.m_uiDepth >> uiMipLevel);

    // Handle pixel row pitch
    GLint iUnpackRowLength = 0;
    if (desc.m_uiRowPitch != 0)
    {
      if (bCompressed)
      {
        // Rows are repacked to tight rows below if the pitch differs; GL has no pitch parameter for compressed data.
      }
      else
      {
        const ezUInt32 uiBytesPerPixel = ezGALResourceFormat::GetBitsPerElement(m_Description.m_Format) / 8;
        if (uiBytesPerPixel > 0)
        {
          iUnpackRowLength = static_cast<GLint>(desc.m_uiRowPitch / uiBytesPerPixel);
        }
      }
    }

    auto cleanupPixelStore = ezMakeScopeExit([&] {
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
      glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
    });

    // Handle 3D/array slice pitch. GL_UNPACK_IMAGE_HEIGHT is measured in rows.
    GLint iUnpackImageHeight = 0;
    if (desc.m_uiSlicePitch != 0 && desc.m_uiRowPitch != 0)
    {
      if (desc.m_uiSlicePitch % desc.m_uiRowPitch != 0)
      {
        ezLog::Error("Slice pitch ({0}) is not a multiple of the row pitch ({1}) for format {2}.", desc.m_uiSlicePitch, desc.m_uiRowPitch, m_Description.m_Format.GetValue());
        return EZ_FAILURE;
      }
      iUnpackImageHeight = static_cast<GLint>(desc.m_uiSlicePitch / desc.m_uiRowPitch);
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, iUnpackRowLength);
    if (iUnpackImageHeight != 0)
    {
      glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, iUnpackImageHeight);
    }

    const void* pData = desc.m_pData.GetPtr();

    // Repack padded rows to tight rows; GL compressed uploads require tightly packed data.
    ezDynamicArray<ezUInt8> repacked;
    if (bCompressed && desc.m_uiRowPitch != 0)
    {
      const ezUInt32 uiBytesPerRow = ((uiWidth + 3) / 4) * uiBlockBytes;
      if (desc.m_uiRowPitch != uiBytesPerRow)
      {
        RepackCompressedToTight(desc, uiWidth, uiHeight, eTarget == GL_TEXTURE_3D ? uiDepth : 1, uiBlockBytes, repacked);
        pData = repacked.GetData();
      }
    }

    if (bCompressed)
    {
      const ezUInt32 uiBlockWidth = 4, uiBlockHeight = 4;
      const GLsizei uiImageSize = static_cast<GLsizei>((((uiWidth + uiBlockWidth - 1) / uiBlockWidth) * ((uiHeight + uiBlockHeight - 1) / uiBlockHeight)) * uiBlockBytes * (eTarget == GL_TEXTURE_3D ? uiDepth : 1));

      switch (eTarget)
      {
        case GL_TEXTURE_3D:
          if (glCompressedTextureSubImage3D != nullptr)
            glCompressedTextureSubImage3D(m_uiTexture, uiMipLevel, 0, 0, 0, uiWidth, uiHeight, uiDepth, ezMappingTables::ToGLTextureInternalFormat(m_Description.m_Format), uiImageSize, pData);
          else
            glCompressedTexSubImage3D(eTarget, uiMipLevel, 0, 0, 0, uiWidth, uiHeight, uiDepth, ezMappingTables::ToGLTextureInternalFormat(m_Description.m_Format), uiImageSize, pData);
          break;
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_CUBE_MAP_ARRAY:
          if (glCompressedTextureSubImage3D != nullptr)
            glCompressedTextureSubImage3D(m_uiTexture, uiMipLevel, 0, 0, static_cast<GLint>(uiSlice), uiWidth, uiHeight, 1, ezMappingTables::ToGLTextureInternalFormat(m_Description.m_Format), uiImageSize, pData);
          else
            glCompressedTexSubImage3D(eTarget, uiMipLevel, 0, 0, static_cast<GLint>(uiSlice), uiWidth, uiHeight, 1, ezMappingTables::ToGLTextureInternalFormat(m_Description.m_Format), uiImageSize, pData);
          break;
        default: // 2D and cube maps (cube maps treat each face as a slice)
          if (glCompressedTextureSubImage2D != nullptr && eTarget != GL_TEXTURE_CUBE_MAP)
            glCompressedTextureSubImage2D(m_uiTexture, uiMipLevel, 0, 0, uiWidth, uiHeight, ezMappingTables::ToGLTextureInternalFormat(m_Description.m_Format), uiImageSize, pData);
          else
            glCompressedTexSubImage2D(FaceTarget(eTarget, uiSlice), uiMipLevel, 0, 0, uiWidth, uiHeight, ezMappingTables::ToGLTextureInternalFormat(m_Description.m_Format), uiImageSize, pData);
          break;
      }
      return EZ_SUCCESS;
    }

    switch (eTarget)
    {
      case GL_TEXTURE_3D:
        if (glTextureSubImage3D != nullptr)
          glTextureSubImage3D(m_uiTexture, uiMipLevel, 0, 0, 0, uiWidth, uiHeight, uiDepth, eFormat, eType, pData);
        else
          glTexSubImage3D(eTarget, uiMipLevel, 0, 0, 0, uiWidth, uiHeight, uiDepth, eFormat, eType, pData);
        break;
      case GL_TEXTURE_2D_ARRAY:
      case GL_TEXTURE_CUBE_MAP_ARRAY:
        if (glTextureSubImage3D != nullptr)
          glTextureSubImage3D(m_uiTexture, uiMipLevel, 0, 0, static_cast<GLint>(uiSlice), uiWidth, uiHeight, 1, eFormat, eType, pData);
        else
          glTexSubImage3D(eTarget, uiMipLevel, 0, 0, static_cast<GLint>(uiSlice), uiWidth, uiHeight, 1, eFormat, eType, pData);
        break;
      default: // 2D and cube maps
        if (glTextureSubImage2D != nullptr && eTarget != GL_TEXTURE_CUBE_MAP)
          glTextureSubImage2D(m_uiTexture, uiMipLevel, 0, 0, uiWidth, uiHeight, eFormat, eType, pData);
        else
          glTexSubImage2D(FaceTarget(eTarget, uiSlice), uiMipLevel, 0, 0, uiWidth, uiHeight, eFormat, eType, pData);
        break;
    }

    return EZ_SUCCESS;
  };

  for (ezUInt32 slice = 0; slice < uiArraySize; slice++)
  {
    for (ezUInt32 level = 0; level < uiMipLevels; level++)
    {
      if (uploadSlice(slice, level).Failed())
        return EZ_FAILURE;
    }
  }

  return EZ_SUCCESS;
}

ezResult ezGALTextureOpenGL::UploadSubImage(const ezGALTextureSubresource& destinationSubResource, const ezBoundingBoxu32& destinationBox, const ezGALSystemMemoryDescription& sourceData) const
{
  if (m_bMSAA)
  {
    ezLog::Error("Partial updates of MSAA textures are not supported.");
    return EZ_FAILURE;
  }

  const ezGALTexture* pParent = static_cast<const ezGALTexture*>(GetParentResource());
  EZ_IGNORE_UNUSED(pParent);

  const ezUInt32 uiMipLevel = destinationSubResource.m_uiMipLevel;
  const ezUInt32 uiSlice = destinationSubResource.m_uiArraySlice;

  const ezUInt32 uiFullWidth = ezMath::Max<ezUInt32>(1, m_Description.m_uiWidth >> uiMipLevel);
  const ezUInt32 uiFullHeight = ezMath::Max<ezUInt32>(1, m_Description.m_uiHeight >> uiMipLevel);
  const ezUInt32 uiFullDepth = ezMath::Max<ezUInt32>(1, m_Description.m_uiDepth >> uiMipLevel);

  const ezUInt32 uiX = destinationBox.m_vMin.x;
  const ezUInt32 uiY = destinationBox.m_vMin.y;
  const ezUInt32 uiZ = destinationBox.m_vMin.z;
  const ezUInt32 uiWidth = destinationBox.m_vMax.x - uiX;
  const ezUInt32 uiHeight = destinationBox.m_vMax.y - uiY;
  const ezUInt32 uiDepth = destinationBox.m_vMax.z - uiZ;

  if (uiWidth == 0 || uiHeight == 0 || uiDepth == 0)
  {
    ezLog::Error("Update box has a zero extent.");
    return EZ_FAILURE;
  }
  if (uiX + uiWidth > uiFullWidth || uiY + uiHeight > uiFullHeight || uiZ + uiDepth > uiFullDepth)
  {
    ezLog::Error("Update box exceeds the mip extent.");
    return EZ_FAILURE;
  }

  const GLenum eTarget = m_eTarget;
  const auto FaceTarget = [](GLenum target, ezUInt32 uiFace) -> GLenum { return target == GL_TEXTURE_CUBE_MAP ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(uiFace) : target; };

  glBindTexture(eTarget, m_uiTexture);
  auto cleanupBind = ezMakeScopeExit([&] { glBindTexture(eTarget, 0); });

  const GLenum eFormat = ezMappingTables::ToGLTextureFormat(m_Description.m_Format);
  const GLenum eType = ezMappingTables::ToGLTextureType(m_Description.m_Format);
  const bool bCompressed = ezGALResourceFormat::IsBlockCompressed(m_Description.m_Format);

  ezUInt32 uiBlockBytes = 0;
  ezMappingTables::IsBlockCompressedFormat(m_Description.m_Format, uiBlockBytes);

  GLint iUnpackRowLength = 0;
  GLint iUnpackImageHeight = 0;
  if (sourceData.m_uiRowPitch != 0)
  {
    if (bCompressed)
    {
      if (uiX != 0 || uiY != 0 || uiZ != 0 || uiWidth != uiFullWidth || uiHeight != uiFullHeight)
      {
        ezLog::Error("Compressed textures only support full-mip updates from the origin.");
        return EZ_FAILURE;
      }
      // Rows are repacked to tight rows below if the pitch differs; GL has no pitch parameter for compressed data.
    }
    else
    {
      const ezUInt32 uiBytesPerPixel = ezGALResourceFormat::GetBitsPerElement(m_Description.m_Format) / 8;
      if (uiBytesPerPixel == 0)
      {
        ezLog::Error("Format {0} has an invalid element size.", m_Description.m_Format.GetValue());
        return EZ_FAILURE;
      }
      if (sourceData.m_uiRowPitch % uiBytesPerPixel != 0)
      {
        ezLog::Error("Row pitch ({0}) is not a multiple of the element size for format {1}.", sourceData.m_uiRowPitch, m_Description.m_Format.GetValue());
        return EZ_FAILURE;
      }
      iUnpackRowLength = static_cast<GLint>(sourceData.m_uiRowPitch / uiBytesPerPixel);
      if (sourceData.m_uiSlicePitch != 0)
      {
        if (sourceData.m_uiSlicePitch % sourceData.m_uiRowPitch != 0)
        {
          ezLog::Error("Slice pitch ({0}) is not a multiple of the row pitch ({1}).", sourceData.m_uiSlicePitch, sourceData.m_uiRowPitch);
          return EZ_FAILURE;
        }
        iUnpackImageHeight = static_cast<GLint>(sourceData.m_uiSlicePitch / sourceData.m_uiRowPitch);
      }
    }
  }

  auto cleanupPixelStore = ezMakeScopeExit([&] {
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
  });
  glPixelStorei(GL_UNPACK_ROW_LENGTH, iUnpackRowLength);
  if (iUnpackImageHeight != 0)
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, iUnpackImageHeight);

  const void* pData = sourceData.m_pData.GetPtr();

  // Repack padded rows to tight rows; GL compressed uploads require tightly packed data.
  ezDynamicArray<ezUInt8> repacked;
  if (bCompressed && sourceData.m_uiRowPitch != 0)
  {
    const ezUInt32 uiBytesPerRow = ((uiWidth + 3) / 4) * uiBlockBytes;
    if (sourceData.m_uiRowPitch != uiBytesPerRow)
    {
      RepackCompressedToTight(sourceData, uiWidth, uiHeight, eTarget == GL_TEXTURE_3D ? uiDepth : 1, uiBlockBytes, repacked);
      pData = repacked.GetData();
    }
  }

  if (bCompressed)
  {
    const GLsizei uiImageSize = static_cast<GLsizei>((((uiWidth + 3) / 4) * ((uiHeight + 3) / 4)) * uiBlockBytes * (eTarget == GL_TEXTURE_3D ? uiDepth : 1));
    const GLenum eInternal = ezMappingTables::ToGLTextureInternalFormat(m_Description.m_Format);
    switch (eTarget)
    {
      case GL_TEXTURE_3D:
        glCompressedTexSubImage3D(eTarget, uiMipLevel, 0, 0, 0, uiWidth, uiHeight, uiDepth, eInternal, uiImageSize, pData);
        break;
      case GL_TEXTURE_2D_ARRAY:
      case GL_TEXTURE_CUBE_MAP_ARRAY:
        glCompressedTexSubImage3D(eTarget, uiMipLevel, 0, 0, static_cast<GLint>(uiSlice), uiWidth, uiHeight, 1, eInternal, uiImageSize, pData);
        break;
      default:
        glCompressedTexSubImage2D(FaceTarget(eTarget, uiSlice), uiMipLevel, 0, 0, uiWidth, uiHeight, eInternal, uiImageSize, pData);
        break;
    }
    return EZ_SUCCESS;
  }

  switch (eTarget)
  {
    case GL_TEXTURE_3D:
      glTexSubImage3D(eTarget, uiMipLevel, static_cast<GLint>(uiX), static_cast<GLint>(uiY), static_cast<GLint>(uiZ), uiWidth, uiHeight, uiDepth, eFormat, eType, pData);
      break;
    case GL_TEXTURE_2D_ARRAY:
    case GL_TEXTURE_CUBE_MAP_ARRAY:
      glTexSubImage3D(eTarget, uiMipLevel, static_cast<GLint>(uiX), static_cast<GLint>(uiY), static_cast<GLint>(uiSlice), uiWidth, uiHeight, 1, eFormat, eType, pData);
      break;
    default: // 2D and cube maps
      glTexSubImage2D(FaceTarget(eTarget, uiSlice), uiMipLevel, static_cast<GLint>(uiX), static_cast<GLint>(uiY), uiWidth, uiHeight, eFormat, eType, pData);
      break;
  }

  return EZ_SUCCESS;
}

ezResult ezGALTextureOpenGL::DeInitPlatform(ezGALDevice* pDevice)
{
  EZ_IGNORE_UNUSED(pDevice);

  if (m_uiTexture != 0)
  {
    const GLuint uiTexture = m_uiTexture;
    glDeleteTextures(1, &uiTexture);
    m_uiTexture = 0;
  }
  return EZ_SUCCESS;
}
