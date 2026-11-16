#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <RendererFoundation/Descriptors/Enumerations.h>
#include <RendererFoundation/Resources/ResourceFormats.h>
#include <RendererOpenGL/Utils/MappingTablesOpenGL.h>

namespace ezMappingTables
{
  GLint ToGLCompareFunc(ezGALCompareFunc::Enum func)
  {
    switch (func)
    {
      case ezGALCompareFunc::Never: return GL_NEVER;
      case ezGALCompareFunc::Less: return GL_LESS;
      case ezGALCompareFunc::Equal: return GL_EQUAL;
      case ezGALCompareFunc::LessEqual: return GL_LEQUAL;
      case ezGALCompareFunc::Greater: return GL_GREATER;
      case ezGALCompareFunc::NotEqual: return GL_NOTEQUAL;
      case ezGALCompareFunc::GreaterEqual: return GL_GEQUAL;
      case ezGALCompareFunc::Always: return GL_ALWAYS;
      default: return GL_ALWAYS;
    }
  }

  GLint ToGLStencilOp(ezGALStencilOp::Enum op)
  {
    switch (op)
    {
      case ezGALStencilOp::Keep: return GL_KEEP;
      case ezGALStencilOp::Zero: return GL_ZERO;
      case ezGALStencilOp::Replace: return GL_REPLACE;
      case ezGALStencilOp::IncrementSaturated: return GL_INCR;
      case ezGALStencilOp::DecrementSaturated: return GL_DECR;
      case ezGALStencilOp::Invert: return GL_INVERT;
      case ezGALStencilOp::Increment: return GL_INCR_WRAP;
      case ezGALStencilOp::Decrement: return GL_DECR_WRAP;
      default: return GL_KEEP;
    }
  }

  GLint ToGLCullMode(ezGALCullMode::Enum mode)
  {
    switch (mode)
    {
      case ezGALCullMode::Front: return GL_FRONT;
      case ezGALCullMode::Back: return GL_BACK;
      case ezGALCullMode::None: return GL_BACK; // GL_CULL_FACE should be disabled for None
      default: return GL_BACK;
    }
  }

  GLenum ToGLBlendOp(ezGALBlendOp::Enum op)
  {
    switch (op)
    {
      case ezGALBlendOp::Add: return GL_FUNC_ADD;
      case ezGALBlendOp::Subtract: return GL_FUNC_SUBTRACT;
      case ezGALBlendOp::RevSubtract: return GL_FUNC_REVERSE_SUBTRACT;
      case ezGALBlendOp::Min: return GL_MIN;
      case ezGALBlendOp::Max: return GL_MAX;
      default: return GL_FUNC_ADD;
    }
  }

  GLenum ToGLBlendFactor(ezGALBlend::Enum factor)
  {
    switch (factor)
    {
      case ezGALBlend::Zero: return GL_ZERO;
      case ezGALBlend::One: return GL_ONE;
      case ezGALBlend::SrcColor: return GL_SRC_COLOR;
      case ezGALBlend::InvSrcColor: return GL_ONE_MINUS_SRC_COLOR;
      case ezGALBlend::SrcAlpha: return GL_SRC_ALPHA;
      case ezGALBlend::InvSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
      case ezGALBlend::DestAlpha: return GL_DST_ALPHA;
      case ezGALBlend::InvDestAlpha: return GL_ONE_MINUS_DST_ALPHA;
      case ezGALBlend::DestColor: return GL_DST_COLOR;
      case ezGALBlend::InvDestColor: return GL_ONE_MINUS_DST_COLOR;
      case ezGALBlend::SrcAlphaSaturated: return GL_SRC_ALPHA_SATURATE;
      case ezGALBlend::BlendFactor: return GL_CONSTANT_COLOR;
      case ezGALBlend::InvBlendFactor: return GL_ONE_MINUS_CONSTANT_COLOR;
      default: return GL_ONE;
    }
  }

  GLenum ToGLPrimitiveTopology(ezGALPrimitiveTopology::Enum topology)
  {
    switch (topology)
    {
      case ezGALPrimitiveTopology::Points: return GL_POINTS;
      case ezGALPrimitiveTopology::Lines: return GL_LINES;
      case ezGALPrimitiveTopology::Triangles: return GL_TRIANGLES;
      case ezGALPrimitiveTopology::TriangleStrip: return GL_TRIANGLE_STRIP;
      default: return GL_TRIANGLES;
    }
  }

  GLint ToGLAddressMode(ezImageAddressMode::Enum mode)
  {
    switch (mode)
    {
      case ezImageAddressMode::Repeat: return GL_REPEAT;
      case ezImageAddressMode::Clamp: return GL_CLAMP_TO_EDGE;
      case ezImageAddressMode::ClampBorder: return GL_CLAMP_TO_BORDER;
      case ezImageAddressMode::Mirror: return GL_MIRRORED_REPEAT;
      default: return GL_REPEAT;
    }
  }

  GLint ToGLTexMinFilter(ezGALTextureFilterMode::Enum min, ezGALTextureFilterMode::Enum mip)
  {
    if (min == ezGALTextureFilterMode::Point)
    {
      return mip == ezGALTextureFilterMode::Point ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR;
    }
    else if (min == ezGALTextureFilterMode::Anisotropic)
    {
      // anisotropy is handled by the texture parameter GL_TEXTURE_MAX_ANISOTROPY_EXT; use trilinear min filter
      return GL_LINEAR_MIPMAP_LINEAR;
    }
    else // Linear
    {
      return mip == ezGALTextureFilterMode::Point ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
    }
  }

  GLint ToGLTexMagFilter(ezGALTextureFilterMode::Enum mag)
  {
    return mag == ezGALTextureFilterMode::Point ? GL_NEAREST : GL_LINEAR;
  }

  bool ToGLVertexAttributeFormat(ezGALResourceFormat::Enum format, GLint& out_components, GLenum& out_type, GLboolean& out_normalized, bool& out_integer)
  {
    out_normalized = GL_FALSE;
    out_integer = false;
    out_components = 0;
    out_type = GL_FLOAT;

    switch (format)
    {
      case ezGALResourceFormat::RGBAFloat: out_components = 4; out_type = GL_FLOAT; return true;
      case ezGALResourceFormat::RGBAUInt: out_components = 4; out_type = GL_UNSIGNED_INT; out_integer = true; return true;
      case ezGALResourceFormat::RGBAInt: out_components = 4; out_type = GL_INT; out_integer = true; return true;
      case ezGALResourceFormat::RGBFloat: out_components = 3; out_type = GL_FLOAT; return true;
      case ezGALResourceFormat::RGBUInt: out_components = 3; out_type = GL_UNSIGNED_INT; out_integer = true; return true;
      case ezGALResourceFormat::RGBInt: out_components = 3; out_type = GL_INT; out_integer = true; return true;
      case ezGALResourceFormat::B5G6R5UNormalized: out_components = 3; out_type = GL_UNSIGNED_SHORT_5_6_5; out_normalized = GL_TRUE; return true;
      // GL_EXT_vertex_array_bgra is required for GL_BGRA vertex attribute pointers; without it GL raises GL_INVALID_ENUM.
      case ezGALResourceFormat::BGRAUByteNormalized: out_components = 4; out_type = GL_BGRA; out_normalized = GL_TRUE; return true;
      case ezGALResourceFormat::BGRAUByteNormalizedsRGB: out_components = 4; out_type = GL_UNSIGNED_BYTE; out_normalized = GL_TRUE; return true;
      case ezGALResourceFormat::RGBAHalf: out_components = 4; out_type = GL_HALF_FLOAT; return true;
      case ezGALResourceFormat::RGBAUShort: out_components = 4; out_type = GL_UNSIGNED_SHORT; out_integer = true; return true;
      case ezGALResourceFormat::RGBAUShortNormalized: out_components = 4; out_type = GL_UNSIGNED_SHORT; out_normalized = GL_TRUE; return true;
      case ezGALResourceFormat::RGBAShort: out_components = 4; out_type = GL_SHORT; out_integer = true; return true;
      case ezGALResourceFormat::RGBAShortNormalized: out_components = 4; out_type = GL_SHORT; out_normalized = GL_TRUE; return true;
      case ezGALResourceFormat::RGFloat: out_components = 2; out_type = GL_FLOAT; return true;
      case ezGALResourceFormat::RGUInt: out_components = 2; out_type = GL_UNSIGNED_INT; out_integer = true; return true;
      case ezGALResourceFormat::RGInt: out_components = 2; out_type = GL_INT; out_integer = true; return true;
      case ezGALResourceFormat::RGB10A2UInt: out_components = 4; out_type = GL_UNSIGNED_INT_2_10_10_10_REV; out_integer = true; return true;
      case ezGALResourceFormat::RGB10A2UIntNormalized: out_components = 4; out_type = GL_UNSIGNED_INT_2_10_10_10_REV; out_normalized = GL_TRUE; return true;
      case ezGALResourceFormat::RG11B10Float: out_components = 3; out_type = GL_UNSIGNED_INT_10F_11F_11F_REV; return true;
      case ezGALResourceFormat::RGBAUByteNormalized: out_components = 4; out_type = GL_UNSIGNED_BYTE; out_normalized = GL_TRUE; return true;
      case ezGALResourceFormat::RGBAUByteNormalizedsRGB: out_components = 4; out_type = GL_UNSIGNED_BYTE; out_normalized = GL_TRUE; return true;
      case ezGALResourceFormat::RGBAUByte: out_components = 4; out_type = GL_UNSIGNED_BYTE; out_integer = true; return true;
      case ezGALResourceFormat::RGBAByteNormalized: out_components = 4; out_type = GL_BYTE; out_normalized = GL_TRUE; return true;
      case ezGALResourceFormat::RGBAByte: out_components = 4; out_type = GL_BYTE; out_integer = true; return true;
      case ezGALResourceFormat::RGHalf: out_components = 2; out_type = GL_HALF_FLOAT; return true;
      case ezGALResourceFormat::RGUShort: out_components = 2; out_type = GL_UNSIGNED_SHORT; out_integer = true; return true;
      case ezGALResourceFormat::RGUShortNormalized: out_components = 2; out_type = GL_UNSIGNED_SHORT; out_normalized = GL_TRUE; return true;
      case ezGALResourceFormat::RGShort: out_components = 2; out_type = GL_SHORT; out_integer = true; return true;
      case ezGALResourceFormat::RGShortNormalized: out_components = 2; out_type = GL_SHORT; out_normalized = GL_TRUE; return true;
      case ezGALResourceFormat::RGUByte: out_components = 2; out_type = GL_UNSIGNED_BYTE; out_integer = true; return true;
      case ezGALResourceFormat::RGUByteNormalized: out_components = 2; out_type = GL_UNSIGNED_BYTE; out_normalized = GL_TRUE; return true;
      case ezGALResourceFormat::RGByte: out_components = 2; out_type = GL_BYTE; out_integer = true; return true;
      case ezGALResourceFormat::RGByteNormalized: out_components = 2; out_type = GL_BYTE; out_normalized = GL_TRUE; return true;
      case ezGALResourceFormat::RFloat: out_components = 1; out_type = GL_FLOAT; return true;
      case ezGALResourceFormat::RUInt: out_components = 1; out_type = GL_UNSIGNED_INT; out_integer = true; return true;
      case ezGALResourceFormat::RInt: out_components = 1; out_type = GL_INT; out_integer = true; return true;
      case ezGALResourceFormat::RHalf: out_components = 1; out_type = GL_HALF_FLOAT; return true;
      case ezGALResourceFormat::RUShort: out_components = 1; out_type = GL_UNSIGNED_SHORT; out_integer = true; return true;
      case ezGALResourceFormat::RUShortNormalized: out_components = 1; out_type = GL_UNSIGNED_SHORT; out_normalized = GL_TRUE; return true;
      case ezGALResourceFormat::RShort: out_components = 1; out_type = GL_SHORT; out_integer = true; return true;
      case ezGALResourceFormat::RShortNormalized: out_components = 1; out_type = GL_SHORT; out_normalized = GL_TRUE; return true;
      case ezGALResourceFormat::RUByte: out_components = 1; out_type = GL_UNSIGNED_BYTE; out_integer = true; return true;
      case ezGALResourceFormat::RUByteNormalized: out_components = 1; out_type = GL_UNSIGNED_BYTE; out_normalized = GL_TRUE; return true;
      case ezGALResourceFormat::RByte: out_components = 1; out_type = GL_BYTE; out_integer = true; return true;
      case ezGALResourceFormat::RByteNormalized: out_components = 1; out_type = GL_BYTE; out_normalized = GL_TRUE; return true;
      default: return false;
    }
  }

  GLenum GetTexelBufferInternalFormat(ezGALResourceFormat::Enum format)
  {
    switch (format)
    {
      case ezGALResourceFormat::RGBAFloat: return GL_RGBA32F;
      case ezGALResourceFormat::RGBAUInt: return GL_RGBA32UI;
      case ezGALResourceFormat::RGBAInt: return GL_RGBA32I;
      case ezGALResourceFormat::RGBFloat: return GL_RGB32F;
      case ezGALResourceFormat::RGBUInt: return GL_RGB32UI;
      case ezGALResourceFormat::RGBInt: return GL_RGB32I;
      case ezGALResourceFormat::RGBAHalf: return GL_RGBA16F;
      case ezGALResourceFormat::RGBAUShort: return GL_RGBA16UI;
      case ezGALResourceFormat::RGBAUShortNormalized: return GL_RGBA16;
      case ezGALResourceFormat::RGBAShort: return GL_RGBA16I;
      case ezGALResourceFormat::RGBAShortNormalized: return GL_RGBA16_SNORM;
      case ezGALResourceFormat::RGFloat: return GL_RG32F;
      case ezGALResourceFormat::RGUInt: return GL_RG32UI;
      case ezGALResourceFormat::RGInt: return GL_RG32I;
      case ezGALResourceFormat::RGBAUByteNormalized: return GL_RGBA8;
      case ezGALResourceFormat::RGBAUByteNormalizedsRGB: return GL_SRGB8_ALPHA8;
      case ezGALResourceFormat::RGBAUByte: return GL_RGBA8UI;
      case ezGALResourceFormat::RGHalf: return GL_RG16F;
      case ezGALResourceFormat::RGUShort: return GL_RG16UI;
      case ezGALResourceFormat::RGUShortNormalized: return GL_RG16;
      case ezGALResourceFormat::RGShort: return GL_RG16I;
      case ezGALResourceFormat::RGUByte: return GL_RG8UI;
      case ezGALResourceFormat::RGUByteNormalized: return GL_RG8;
      case ezGALResourceFormat::RFloat: return GL_R32F;
      case ezGALResourceFormat::RUInt: return GL_R32UI;
      case ezGALResourceFormat::RInt: return GL_R32I;
      case ezGALResourceFormat::RHalf: return GL_R16F;
      case ezGALResourceFormat::RUShort: return GL_R16UI;
      case ezGALResourceFormat::RUShortNormalized: return GL_R16;
      case ezGALResourceFormat::RGByte: return GL_RG8I;
      case ezGALResourceFormat::RUByte: return GL_R8UI;
      case ezGALResourceFormat::RUByteNormalized: return GL_R8;
      default: return 0;
    }
  }

  GLenum GetImageFormat(ezGALResourceFormat::Enum format)
  {
    // image formats must be unsized or a known sized format, reuse the texture internal formats
    switch (format)
    {
      case ezGALResourceFormat::RGBAFloat: return GL_RGBA32F;
      case ezGALResourceFormat::RGBAUInt: return GL_RGBA32UI;
      case ezGALResourceFormat::RGBFloat:
        return GL_RGB32F;
      case ezGALResourceFormat::RGBUInt:
        return GL_RGB32UI;
      case ezGALResourceFormat::RGBInt:
        return GL_RGB32I;
      case ezGALResourceFormat::RGBAInt: return GL_RGBA32I;
      case ezGALResourceFormat::RGBAHalf: return GL_RGBA16F;
      case ezGALResourceFormat::RGBAUShort: return GL_RGBA16UI;
      case ezGALResourceFormat::RGBAUShortNormalized: return GL_RGBA16;
      case ezGALResourceFormat::RGBAShort: return GL_RGBA16I;
      case ezGALResourceFormat::RGBAShortNormalized:
        return GL_RGBA16_SNORM;
      case ezGALResourceFormat::RGFloat: return GL_RG32F;
      case ezGALResourceFormat::RGUInt: return GL_RG32UI;
      case ezGALResourceFormat::RGInt: return GL_RG32I;
      case ezGALResourceFormat::RGBAUByteNormalized: return GL_RGBA8;
      case ezGALResourceFormat::BGRAUByteNormalized:
        return GL_RGBA8;                                                  // the texture is internally GL_RGBA8; GL has no separate BGRA image format
      case ezGALResourceFormat::RGBAUByteNormalizedsRGB: return GL_RGBA8; // images can't be srgb-qualified, use unorm
      case ezGALResourceFormat::RGBAUByte: return GL_RGBA8UI;
      case ezGALResourceFormat::RGBAByteNormalized: return GL_RGBA8_SNORM;
      case ezGALResourceFormat::RGBAByte: return GL_RGBA8I;
      case ezGALResourceFormat::RGHalf: return GL_RG16F;
      case ezGALResourceFormat::RGUShort: return GL_RG16UI;
      case ezGALResourceFormat::RGUShortNormalized: return GL_RG16;
      case ezGALResourceFormat::RGShort: return GL_RG16I;
      case ezGALResourceFormat::RGShortNormalized:
        return GL_RG16_SNORM;
      case ezGALResourceFormat::RGUByte: return GL_RG8UI;
      case ezGALResourceFormat::RGUByteNormalized: return GL_RG8;
      case ezGALResourceFormat::RGByte:
        return GL_RG8I;
      case ezGALResourceFormat::RGByteNormalized:
        return GL_RG8_SNORM;
      case ezGALResourceFormat::RFloat: return GL_R32F;
      case ezGALResourceFormat::RUInt: return GL_R32UI;
      case ezGALResourceFormat::RInt: return GL_R32I;
      case ezGALResourceFormat::RHalf: return GL_R16F;
      case ezGALResourceFormat::RUShort: return GL_R16UI;
      case ezGALResourceFormat::RUShortNormalized: return GL_R16;
      case ezGALResourceFormat::RByte: return GL_R8I;
      case ezGALResourceFormat::RUByte: return GL_R8UI;
      case ezGALResourceFormat::RUByteNormalized: return GL_R8;
      case ezGALResourceFormat::RByteNormalized:
        return GL_R8_SNORM;
      case ezGALResourceFormat::AUByteNormalized:
        return GL_R8;
      default: return 0;
    }
  }
} // namespace ezMappingTables

// #####################################
// ## Texture formats
// #####################################
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#  define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#  define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#  define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F3
#  define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4F
#  define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F5
#  define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C51
#endif

namespace ezMappingTables
{
  GLenum ToGLTextureInternalFormat(ezGALResourceFormat::Enum format)
  {
    switch (format)
    {
      case ezGALResourceFormat::RGBAFloat: return GL_RGBA32F;
      case ezGALResourceFormat::RGBAUInt: return GL_RGBA32UI;
      case ezGALResourceFormat::RGBAInt: return GL_RGBA32I;

      case ezGALResourceFormat::RGBFloat: return GL_RGB32F;
      case ezGALResourceFormat::RGBUInt: return GL_RGB32UI;
      case ezGALResourceFormat::RGBInt: return GL_RGB32I;

      case ezGALResourceFormat::B5G6R5UNormalized: return GL_RGB565;
      case ezGALResourceFormat::BGRAUByteNormalized: return GL_RGBA8;
      case ezGALResourceFormat::BGRAUByteNormalizedsRGB: return GL_SRGB8_ALPHA8;

      case ezGALResourceFormat::RGBAHalf: return GL_RGBA16F;
      case ezGALResourceFormat::RGBAUShort: return GL_RGBA16UI;
      case ezGALResourceFormat::RGBAUShortNormalized: return GL_RGBA16;
      case ezGALResourceFormat::RGBAShort: return GL_RGBA16I;
      case ezGALResourceFormat::RGBAShortNormalized: return GL_RGBA16_SNORM;

      case ezGALResourceFormat::RGFloat: return GL_RG32F;
      case ezGALResourceFormat::RGUInt: return GL_RG32UI;
      case ezGALResourceFormat::RGInt: return GL_RG32I;

      case ezGALResourceFormat::RGB10A2UInt: return GL_RGB10_A2UI;
      case ezGALResourceFormat::RGB10A2UIntNormalized: return GL_RGB10_A2;
      case ezGALResourceFormat::RG11B10Float: return GL_R11F_G11F_B10F;

      case ezGALResourceFormat::RGBAUByteNormalized: return GL_RGBA8;
      case ezGALResourceFormat::RGBAUByteNormalizedsRGB: return GL_SRGB8_ALPHA8;
      case ezGALResourceFormat::RGBAUByte: return GL_RGBA8UI;
      case ezGALResourceFormat::RGBAByteNormalized: return GL_RGBA8_SNORM;
      case ezGALResourceFormat::RGBAByte: return GL_RGBA8I;

      case ezGALResourceFormat::RGHalf: return GL_RG16F;
      case ezGALResourceFormat::RGUShort: return GL_RG16UI;
      case ezGALResourceFormat::RGUShortNormalized: return GL_RG16;
      case ezGALResourceFormat::RGShort: return GL_RG16I;
      case ezGALResourceFormat::RGShortNormalized: return GL_RG16_SNORM;
      case ezGALResourceFormat::RGUByte: return GL_RG8UI;
      case ezGALResourceFormat::RGUByteNormalized: return GL_RG8;
      case ezGALResourceFormat::RGByte: return GL_RG8I;
      case ezGALResourceFormat::RGByteNormalized: return GL_RG8_SNORM;

      case ezGALResourceFormat::DFloat: return GL_DEPTH_COMPONENT32F;
      case ezGALResourceFormat::D16: return GL_DEPTH_COMPONENT16;
      case ezGALResourceFormat::D24S8: return GL_DEPTH24_STENCIL8;
      case ezGALResourceFormat::DFloatS8: return GL_DEPTH32F_STENCIL8;

      case ezGALResourceFormat::RFloat: return GL_R32F;
      case ezGALResourceFormat::RUInt: return GL_R32UI;
      case ezGALResourceFormat::RInt: return GL_R32I;
      case ezGALResourceFormat::RHalf: return GL_R16F;
      case ezGALResourceFormat::RUShort: return GL_R16UI;
      case ezGALResourceFormat::RUShortNormalized: return GL_R16;
      case ezGALResourceFormat::RShort: return GL_R16I;
      case ezGALResourceFormat::RShortNormalized: return GL_R16_SNORM;
      case ezGALResourceFormat::RUByte: return GL_R8UI;
      case ezGALResourceFormat::RUByteNormalized: return GL_R8;
      case ezGALResourceFormat::RByte: return GL_R8I;
      case ezGALResourceFormat::RByteNormalized: return GL_R8_SNORM;

      case ezGALResourceFormat::AUByteNormalized: return GL_R8;

      case ezGALResourceFormat::BC1: return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
      case ezGALResourceFormat::BC1sRGB: return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
      case ezGALResourceFormat::BC2: return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
      case ezGALResourceFormat::BC2sRGB: return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
      case ezGALResourceFormat::BC3: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
      case ezGALResourceFormat::BC3sRGB: return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
      case ezGALResourceFormat::BC4UNormalized: return GL_COMPRESSED_RED_RGTC1;
      case ezGALResourceFormat::BC4Normalized: return GL_COMPRESSED_SIGNED_RED_RGTC1;
      case ezGALResourceFormat::BC5UNormalized: return GL_COMPRESSED_RG_RGTC2;
      case ezGALResourceFormat::BC5Normalized: return GL_COMPRESSED_SIGNED_RG_RGTC2;
      case ezGALResourceFormat::BC6UFloat: return GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
      case ezGALResourceFormat::BC6Float: return GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT;
      case ezGALResourceFormat::BC7UNormalized: return GL_COMPRESSED_RGBA_BPTC_UNORM;
      case ezGALResourceFormat::BC7UNormalizedsRGB: return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;

      default: return 0;
    }
  }

  GLenum ToGLTextureFormat(ezGALResourceFormat::Enum format)
  {
    switch (format)
    {
      case ezGALResourceFormat::RGBAFloat:
      case ezGALResourceFormat::RGBAUInt:
      case ezGALResourceFormat::RGBAInt:
      case ezGALResourceFormat::RGBAHalf:
      case ezGALResourceFormat::RGBAUShort:
      case ezGALResourceFormat::RGBAUShortNormalized:
      case ezGALResourceFormat::RGBAShort:
      case ezGALResourceFormat::RGBAShortNormalized:
      case ezGALResourceFormat::RGBAUByteNormalized:
      case ezGALResourceFormat::RGBAUByteNormalizedsRGB:
      case ezGALResourceFormat::RGBAUByte:
      case ezGALResourceFormat::RGBAByteNormalized:
      case ezGALResourceFormat::RGBAByte:
      case ezGALResourceFormat::RGB10A2UInt:
      case ezGALResourceFormat::RGB10A2UIntNormalized:
        return ezGALResourceFormat::IsIntegerFormat(format) ? GL_RGBA_INTEGER : GL_RGBA;

      case ezGALResourceFormat::BGRAUByteNormalized:
      case ezGALResourceFormat::BGRAUByteNormalizedsRGB:
        return GL_BGRA;

      case ezGALResourceFormat::RGBFloat:
      case ezGALResourceFormat::RGBUInt:
      case ezGALResourceFormat::RGBInt:
        return ezGALResourceFormat::IsIntegerFormat(format) ? GL_RGB_INTEGER : GL_RGB;

      case ezGALResourceFormat::RG11B10Float:
        return GL_RGB;

      case ezGALResourceFormat::B5G6R5UNormalized:
        return GL_RGB;

      case ezGALResourceFormat::RGFloat:
      case ezGALResourceFormat::RGUInt:
      case ezGALResourceFormat::RGInt:
      case ezGALResourceFormat::RGHalf:
      case ezGALResourceFormat::RGUShort:
      case ezGALResourceFormat::RGUShortNormalized:
      case ezGALResourceFormat::RGShort:
      case ezGALResourceFormat::RGShortNormalized:
      case ezGALResourceFormat::RGUByte:
      case ezGALResourceFormat::RGUByteNormalized:
      case ezGALResourceFormat::RGByte:
      case ezGALResourceFormat::RGByteNormalized:
        return ezGALResourceFormat::IsIntegerFormat(format) ? GL_RG_INTEGER : GL_RG;

      case ezGALResourceFormat::RFloat:
      case ezGALResourceFormat::RUInt:
      case ezGALResourceFormat::RInt:
      case ezGALResourceFormat::RHalf:
      case ezGALResourceFormat::RUShort:
      case ezGALResourceFormat::RUShortNormalized:
      case ezGALResourceFormat::RShort:
      case ezGALResourceFormat::RShortNormalized:
      case ezGALResourceFormat::RUByte:
      case ezGALResourceFormat::RUByteNormalized:
      case ezGALResourceFormat::RByte:
      case ezGALResourceFormat::RByteNormalized:
      case ezGALResourceFormat::AUByteNormalized:
        return ezGALResourceFormat::IsIntegerFormat(format) ? GL_RED_INTEGER : GL_RED;

      case ezGALResourceFormat::DFloat:
      case ezGALResourceFormat::D16:
        return GL_DEPTH_COMPONENT;
      case ezGALResourceFormat::D24S8:
      case ezGALResourceFormat::DFloatS8:
        return GL_DEPTH_STENCIL;

      default: return 0;
    }
  }

  GLenum ToGLTextureType(ezGALResourceFormat::Enum format)
  {
    switch (format)
    {
      case ezGALResourceFormat::RGBAFloat:
      case ezGALResourceFormat::RGBFloat:
      case ezGALResourceFormat::RGFloat:
      case ezGALResourceFormat::RFloat:
      case ezGALResourceFormat::DFloat:
        return GL_FLOAT;

      case ezGALResourceFormat::RGBAUInt:
      case ezGALResourceFormat::RGBUInt:
      case ezGALResourceFormat::RGUInt:
      case ezGALResourceFormat::RUInt:
        return GL_UNSIGNED_INT;
      case ezGALResourceFormat::RGBAInt:
      case ezGALResourceFormat::RGBInt:
      case ezGALResourceFormat::RGInt:
      case ezGALResourceFormat::RInt:
        return GL_INT;

      case ezGALResourceFormat::RGBAHalf:
      case ezGALResourceFormat::RGHalf:
      case ezGALResourceFormat::RHalf:
        return GL_HALF_FLOAT;

      case ezGALResourceFormat::RGBAUShort:
      case ezGALResourceFormat::RGBAUShortNormalized:
      case ezGALResourceFormat::RGUShort:
      case ezGALResourceFormat::RGUShortNormalized:
      case ezGALResourceFormat::RUShort:
      case ezGALResourceFormat::RUShortNormalized:
        return GL_UNSIGNED_SHORT;
      case ezGALResourceFormat::RGBAShort:
      case ezGALResourceFormat::RGBAShortNormalized:
      case ezGALResourceFormat::RGShort:
      case ezGALResourceFormat::RGShortNormalized:
      case ezGALResourceFormat::RShort:
      case ezGALResourceFormat::RShortNormalized:
        return GL_SHORT;

      case ezGALResourceFormat::RGB10A2UInt:
      case ezGALResourceFormat::RGB10A2UIntNormalized:
        return GL_UNSIGNED_INT_2_10_10_10_REV;
      case ezGALResourceFormat::RG11B10Float:
        return GL_UNSIGNED_INT_10F_11F_11F_REV;

      case ezGALResourceFormat::BGRAUByteNormalized:
      case ezGALResourceFormat::BGRAUByteNormalizedsRGB:
      case ezGALResourceFormat::RGBAUByteNormalized:
      case ezGALResourceFormat::RGBAUByteNormalizedsRGB:
      case ezGALResourceFormat::RGBAUByte:
      case ezGALResourceFormat::RGUByte:
      case ezGALResourceFormat::RGUByteNormalized:
      case ezGALResourceFormat::RUByte:
      case ezGALResourceFormat::RUByteNormalized:
      case ezGALResourceFormat::AUByteNormalized:
        return GL_UNSIGNED_BYTE;
      case ezGALResourceFormat::RGBAByteNormalized:
      case ezGALResourceFormat::RGBAByte:
      case ezGALResourceFormat::RGByte:
      case ezGALResourceFormat::RGByteNormalized:
      case ezGALResourceFormat::RByte:
      case ezGALResourceFormat::RByteNormalized:
        return GL_BYTE;

      case ezGALResourceFormat::B5G6R5UNormalized:
        return GL_UNSIGNED_SHORT_5_6_5;

      case ezGALResourceFormat::D16:
        return GL_UNSIGNED_SHORT;
      case ezGALResourceFormat::D24S8:
        return GL_UNSIGNED_INT_24_8;
      case ezGALResourceFormat::DFloatS8:
        return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;

      default: return 0;
    }
  }

  bool IsBlockCompressedFormat(ezGALResourceFormat::Enum format, ezUInt32& out_uiBlockBytes)
  {
    switch (format)
    {
      case ezGALResourceFormat::BC1:
      case ezGALResourceFormat::BC1sRGB:
      case ezGALResourceFormat::BC4UNormalized:
      case ezGALResourceFormat::BC4Normalized:
        out_uiBlockBytes = 8;
        return true;

      case ezGALResourceFormat::BC2:
      case ezGALResourceFormat::BC2sRGB:
      case ezGALResourceFormat::BC3:
      case ezGALResourceFormat::BC3sRGB:
      case ezGALResourceFormat::BC5UNormalized:
      case ezGALResourceFormat::BC5Normalized:
      case ezGALResourceFormat::BC6UFloat:
      case ezGALResourceFormat::BC6Float:
      case ezGALResourceFormat::BC7UNormalized:
      case ezGALResourceFormat::BC7UNormalizedsRGB:
        out_uiBlockBytes = 16;
        return true;

      default:
        out_uiBlockBytes = 0;
        return false;
    }
  }
} // namespace ezMappingTables

namespace ezMappingTables
{
  bool IsImageLoadStoreSupportedFormat(ezGALResourceFormat::Enum format)
  {
    // Conservative whitelist of formats that are practically always usable as image load/store targets in desktop GL.
    switch (format)
    {
      case ezGALResourceFormat::RGBAFloat:
      case ezGALResourceFormat::RGBFloat:
      case ezGALResourceFormat::RGFloat:
      case ezGALResourceFormat::RFloat:
      case ezGALResourceFormat::RGBAHalf:
      case ezGALResourceFormat::RGHalf:
      case ezGALResourceFormat::RHalf:
      case ezGALResourceFormat::RGBAUByteNormalized:
      case ezGALResourceFormat::BGRAUByteNormalized:
      case ezGALResourceFormat::RGUByte:
      case ezGALResourceFormat::RGUByteNormalized:
      case ezGALResourceFormat::RUByte:
      case ezGALResourceFormat::RUByteNormalized:
      case ezGALResourceFormat::RGBAUInt:
      case ezGALResourceFormat::RGUInt:
      case ezGALResourceFormat::RUInt:
      case ezGALResourceFormat::RGBAInt:
      case ezGALResourceFormat::RGInt:
      case ezGALResourceFormat::RInt:
        return true;
      default:
        return false;
    }
  }
} // namespace ezMappingTables
