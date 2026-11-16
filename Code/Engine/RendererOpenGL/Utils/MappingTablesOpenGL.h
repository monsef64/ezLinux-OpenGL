#pragma once

/// Mapping tables from ez GAL enums to OpenGL enums.
/// Shared by the GL state objects, pipelines and the command encoder.

#include <RendererFoundation/Descriptors/Descriptors.h>
#include <RendererFoundation/RendererFoundationDLL.h>
#include <glad/glad.h>

namespace ezMappingTables
{
  GLint ToGLCompareFunc(ezGALCompareFunc::Enum func);
  GLint ToGLStencilOp(ezGALStencilOp::Enum op);
  GLint ToGLCullMode(ezGALCullMode::Enum mode);
  GLenum ToGLBlendOp(ezGALBlendOp::Enum op);
  GLenum ToGLBlendFactor(ezGALBlend::Enum factor);
  GLenum ToGLPrimitiveTopology(ezGALPrimitiveTopology::Enum topology);
  GLint ToGLAddressMode(ezImageAddressMode::Enum mode);
  GLint ToGLTexMinFilter(ezGALTextureFilterMode::Enum min, ezGALTextureFilterMode::Enum mip);
  GLint ToGLTexMagFilter(ezGALTextureFilterMode::Enum mag);
  bool ToGLVertexAttributeFormat(ezGALResourceFormat::Enum format, GLint& out_components, GLenum& out_type, GLboolean& out_normalized, bool& out_integer);
  GLenum GetTexelBufferInternalFormat(ezGALResourceFormat::Enum format);
  GLenum GetImageFormat(ezGALResourceFormat::Enum format);
} // namespace ezMappingTables

namespace ezMappingTables
{
  GLenum ToGLTextureInternalFormat(ezGALResourceFormat::Enum format);
  GLenum ToGLTextureFormat(ezGALResourceFormat::Enum format); ///< External upload (pixel) format. Do not use for compressed formats.
  GLenum ToGLTextureType(ezGALResourceFormat::Enum format);   ///< External upload (pixel) data type. Do not use for compressed formats.
  bool IsBlockCompressedFormat(ezGALResourceFormat::Enum format, ezUInt32& out_uiBlockBytes);
  bool IsImageLoadStoreSupportedFormat(ezGALResourceFormat::Enum format);
} // namespace ezMappingTables
