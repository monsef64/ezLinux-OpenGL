#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <RendererOpenGL/Utils/ConversionUtilsOpenGL.h>

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#  define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#  define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#  define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F3
#  define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F5
#  define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT 0x8C4C
#  define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#  define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4F
#  define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C51
#endif

namespace ezConversionUtilsOpenGL
{
  namespace
  {
    struct PixelFormatInfo
    {
      GLenum m_format;
      GLenum m_type;
    };
  } // namespace

  void FillFormatLookupTable(ezGALFormatLookupTableOpenGL& ref_table)
  {
    using F = ezGALResourceFormat;

    auto set = [&](F::Enum format, GLenum storage, GLenum renderTarget, GLenum depthStencil) {
      ezGALFormatLookupEntryOpenGL entry(storage);
      if (renderTarget != 0)
        entry.RT(renderTarget);
      if (depthStencil != 0)
        entry.DS(depthStencil);
      ref_table.SetFormatInfo(format, entry);
    };

    auto depthOnly = [&](F::Enum format, GLenum storage, GLenum depth) {
      ezGALFormatLookupEntryOpenGL entry(storage);
      entry.D(depth);
      ref_table.SetFormatInfo(format, entry);
    };

    auto depthStencil = [&](F::Enum format, GLenum storage, GLenum depth, GLenum stencil, GLenum ds) {
      ezGALFormatLookupEntryOpenGL entry(storage);
      entry.D(depth);
      entry.S(stencil);
      entry.DS(ds);
      ref_table.SetFormatInfo(format, entry);
    };

    // RGBA32
    set(F::RGBAFloat, GL_RGBA32F, GL_RGBA32F, 0);
    set(F::RGBAUInt, GL_RGBA32UI, GL_RGBA32UI, 0);
    set(F::RGBAInt, GL_RGBA32I, GL_RGBA32I, 0);
    // RGB32
    set(F::RGBFloat, GL_RGB32F, GL_RGB32F, 0);
    set(F::RGBUInt, GL_RGB32UI, GL_RGB32UI, 0);
    set(F::RGBInt, GL_RGB32I, GL_RGB32I, 0);
    // special packed
    set(F::B5G6R5UNormalized, GL_RGB565, GL_RGB565, 0);
    set(F::BGRAUByteNormalized, GL_RGBA8, GL_RGBA8, 0);
    set(F::BGRAUByteNormalizedsRGB, GL_SRGB8_ALPHA8, GL_SRGB8_ALPHA8, 0);
    // 16 bit
    set(F::RGBAHalf, GL_RGBA16F, GL_RGBA16F, 0);
    set(F::RGBAUShort, GL_RGBA16UI, GL_RGBA16UI, 0);
    set(F::RGBAUShortNormalized, GL_RGBA16, GL_RGBA16, 0);
    set(F::RGBAShort, GL_RGBA16I, GL_RGBA16I, 0);
    set(F::RGBAShortNormalized, GL_RGBA16_SNORM, GL_RGBA16_SNORM, 0);
    // RG32
    set(F::RGFloat, GL_RG32F, GL_RG32F, 0);
    set(F::RGUInt, GL_RG32UI, GL_RG32UI, 0);
    set(F::RGInt, GL_RG32I, GL_RG32I, 0);
    // packed 32 bit
    set(F::RGB10A2UInt, GL_RGB10_A2UI, GL_RGB10_A2UI, 0);
    set(F::RGB10A2UIntNormalized, GL_RGB10_A2, GL_RGB10_A2, 0);
    set(F::RG11B10Float, GL_R11F_G11F_B10F, GL_R11F_G11F_B10F, 0);
    // 8 bit
    set(F::RGBAUByteNormalized, GL_RGBA8, GL_RGBA8, 0);
    set(F::RGBAUByteNormalizedsRGB, GL_SRGB8_ALPHA8, GL_SRGB8_ALPHA8, 0);
    set(F::RGBAUByte, GL_RGBA8UI, GL_RGBA8UI, 0);
    set(F::RGBAByteNormalized, GL_RGBA8_SNORM, GL_RGBA8_SNORM, 0);
    set(F::RGBAByte, GL_RGBA8I, GL_RGBA8I, 0);
    // RG16
    set(F::RGHalf, GL_RG16F, GL_RG16F, 0);
    set(F::RGUShort, GL_RG16UI, GL_RG16UI, 0);
    set(F::RGUShortNormalized, GL_RG16, GL_RG16, 0);
    set(F::RGShort, GL_RG16I, GL_RG16I, 0);
    set(F::RGShortNormalized, GL_RG16_SNORM, GL_RG16_SNORM, 0);
    // RG8
    set(F::RGUByte, GL_RG8UI, GL_RG8UI, 0);
    set(F::RGUByteNormalized, GL_RG8, GL_RG8, 0);
    set(F::RGByte, GL_RG8I, GL_RG8I, 0);
    set(F::RGByteNormalized, GL_RG8_SNORM, GL_RG8_SNORM, 0);
    // R32
    set(F::RFloat, GL_R32F, GL_R32F, 0);
    set(F::RUInt, GL_R32UI, GL_R32UI, 0);
    set(F::RInt, GL_R32I, GL_R32I, 0);
    // R16
    set(F::RHalf, GL_R16F, GL_R16F, 0);
    set(F::RUShort, GL_R16UI, GL_R16UI, 0);
    set(F::RUShortNormalized, GL_R16, GL_R16, 0);
    set(F::RShort, GL_R16I, GL_R16I, 0);
    set(F::RShortNormalized, GL_R16_SNORM, GL_R16_SNORM, 0);
    // R8
    set(F::RUByte, GL_R8UI, GL_R8UI, 0);
    set(F::RUByteNormalized, GL_R8, GL_R8, 0);
    set(F::RByte, GL_R8I, GL_R8I, 0);
    set(F::RByteNormalized, GL_R8_SNORM, GL_R8_SNORM, 0);
    // single channel alpha
    set(F::AUByteNormalized, GL_R8, GL_R8, 0);

    // depth / stencil
    depthOnly(F::DFloat, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT32F);
    depthOnly(F::D16, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT16);
    depthStencil(F::D24S8, GL_DEPTH24_STENCIL8, GL_DEPTH_COMPONENT24, GL_STENCIL_INDEX8, GL_DEPTH24_STENCIL8);
    depthStencil(F::DFloatS8, GL_DEPTH32F_STENCIL8, GL_DEPTH_COMPONENT32F, GL_STENCIL_INDEX8, GL_DEPTH32F_STENCIL8);

    // BC1 - BC7
    auto compressed = [&](F::Enum format, GLenum storage) {
      ezGALFormatLookupEntryOpenGL entry(storage);
      entry.RT(0);
      ref_table.SetFormatInfo(format, entry);
    };

    compressed(F::BC1, GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
    compressed(F::BC1sRGB, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT);
    compressed(F::BC2, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
    compressed(F::BC2sRGB, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT);
    compressed(F::BC3, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
    compressed(F::BC3sRGB, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT);
    compressed(F::BC4UNormalized, GL_COMPRESSED_RED_RGTC1);
    compressed(F::BC4Normalized, GL_COMPRESSED_SIGNED_RED_RGTC1);
    compressed(F::BC5UNormalized, GL_COMPRESSED_RG_RGTC2);
    compressed(F::BC5Normalized, GL_COMPRESSED_SIGNED_RG_RGTC2);
    compressed(F::BC6UFloat, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT);
    compressed(F::BC6Float, GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT);
    compressed(F::BC7UNormalized, GL_COMPRESSED_RGBA_BPTC_UNORM);
    compressed(F::BC7UNormalizedsRGB, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM);
  }

  GLenum GetInternalFormat(ezGALResourceFormat::Enum format)
  {
    auto& table = []() -> ezGALFormatLookupTableOpenGL& {
      static ezGALFormatLookupTableOpenGL s_table;
      static bool s_bFilled = false;
      if (!s_bFilled)
      {
        FillFormatLookupTable(s_table);
        s_bFilled = true;
      }
      return s_table;
    }();

    return table.GetFormatInfo(format).m_eStorage;
  }

  bool GetPixelFormat(ezGALResourceFormat::Enum format, GLenum& out_format, GLenum& out_type)
  {
    switch (format)
    {
      // RGBA32
      case ezGALResourceFormat::RGBAFloat: out_format = GL_RGBA; out_type = GL_FLOAT; return true;
      case ezGALResourceFormat::RGBAUInt: out_format = GL_RGBA_INTEGER; out_type = GL_UNSIGNED_INT; return true;
      case ezGALResourceFormat::RGBAInt: out_format = GL_RGBA_INTEGER; out_type = GL_INT; return true;
      // RGB32
      case ezGALResourceFormat::RGBFloat: out_format = GL_RGB; out_type = GL_FLOAT; return true;
      case ezGALResourceFormat::RGBUInt: out_format = GL_RGB_INTEGER; out_type = GL_UNSIGNED_INT; return true;
      case ezGALResourceFormat::RGBInt: out_format = GL_RGB_INTEGER; out_type = GL_INT; return true;
      // special packed
      case ezGALResourceFormat::B5G6R5UNormalized: out_format = GL_RGB; out_type = GL_UNSIGNED_SHORT_5_6_5; return true;
      case ezGALResourceFormat::BGRAUByteNormalized: out_format = GL_BGRA; out_type = GL_UNSIGNED_BYTE; return true;
      case ezGALResourceFormat::BGRAUByteNormalizedsRGB: out_format = GL_BGRA; out_type = GL_UNSIGNED_BYTE; return true;
      // 16 bit
      case ezGALResourceFormat::RGBAHalf: out_format = GL_RGBA; out_type = GL_HALF_FLOAT; return true;
      case ezGALResourceFormat::RGBAUShort: out_format = GL_RGBA_INTEGER; out_type = GL_UNSIGNED_SHORT; return true;
      case ezGALResourceFormat::RGBAUShortNormalized: out_format = GL_RGBA; out_type = GL_UNSIGNED_SHORT; return true;
      case ezGALResourceFormat::RGBAShort: out_format = GL_RGBA_INTEGER; out_type = GL_SHORT; return true;
      case ezGALResourceFormat::RGBAShortNormalized: out_format = GL_RGBA; out_type = GL_SHORT; return true;
      // RG32
      case ezGALResourceFormat::RGFloat: out_format = GL_RG; out_type = GL_FLOAT; return true;
      case ezGALResourceFormat::RGUInt: out_format = GL_RG_INTEGER; out_type = GL_UNSIGNED_INT; return true;
      case ezGALResourceFormat::RGInt: out_format = GL_RG_INTEGER; out_type = GL_INT; return true;
      // packed 32 bit
      case ezGALResourceFormat::RGB10A2UInt: out_format = GL_RGBA_INTEGER; out_type = GL_UNSIGNED_INT_10_10_10_2; return true;
      case ezGALResourceFormat::RGB10A2UIntNormalized: out_format = GL_RGBA; out_type = GL_UNSIGNED_INT_2_10_10_10_REV; return true;
      case ezGALResourceFormat::RG11B10Float: out_format = GL_RGB; out_type = GL_UNSIGNED_INT_10F_11F_11F_REV; return true;
      // 8 bit
      case ezGALResourceFormat::RGBAUByteNormalized: out_format = GL_RGBA; out_type = GL_UNSIGNED_BYTE; return true;
      case ezGALResourceFormat::RGBAUByteNormalizedsRGB: out_format = GL_RGBA; out_type = GL_UNSIGNED_BYTE; return true;
      case ezGALResourceFormat::RGBAUByte: out_format = GL_RGBA_INTEGER; out_type = GL_UNSIGNED_BYTE; return true;
      case ezGALResourceFormat::RGBAByteNormalized: out_format = GL_RGBA; out_type = GL_BYTE; return true;
      case ezGALResourceFormat::RGBAByte: out_format = GL_RGBA_INTEGER; out_type = GL_BYTE; return true;
      // RG16
      case ezGALResourceFormat::RGHalf: out_format = GL_RG; out_type = GL_HALF_FLOAT; return true;
      case ezGALResourceFormat::RGUShort: out_format = GL_RG_INTEGER; out_type = GL_UNSIGNED_SHORT; return true;
      case ezGALResourceFormat::RGUShortNormalized: out_format = GL_RG; out_type = GL_UNSIGNED_SHORT; return true;
      case ezGALResourceFormat::RGShort: out_format = GL_RG_INTEGER; out_type = GL_SHORT; return true;
      case ezGALResourceFormat::RGShortNormalized: out_format = GL_RG; out_type = GL_SHORT; return true;
      // RG8
      case ezGALResourceFormat::RGUByte: out_format = GL_RG_INTEGER; out_type = GL_UNSIGNED_BYTE; return true;
      case ezGALResourceFormat::RGUByteNormalized: out_format = GL_RG; out_type = GL_UNSIGNED_BYTE; return true;
      case ezGALResourceFormat::RGByte: out_format = GL_RG_INTEGER; out_type = GL_BYTE; return true;
      case ezGALResourceFormat::RGByteNormalized: out_format = GL_RG; out_type = GL_BYTE; return true;
      // R32
      case ezGALResourceFormat::RFloat: out_format = GL_RED; out_type = GL_FLOAT; return true;
      case ezGALResourceFormat::RUInt: out_format = GL_RED_INTEGER; out_type = GL_UNSIGNED_INT; return true;
      case ezGALResourceFormat::RInt: out_format = GL_RED_INTEGER; out_type = GL_INT; return true;
      // R16
      case ezGALResourceFormat::RHalf: out_format = GL_RED; out_type = GL_HALF_FLOAT; return true;
      case ezGALResourceFormat::RUShort: out_format = GL_RED_INTEGER; out_type = GL_UNSIGNED_SHORT; return true;
      case ezGALResourceFormat::RUShortNormalized: out_format = GL_RED; out_type = GL_UNSIGNED_SHORT; return true;
      case ezGALResourceFormat::RShort: out_format = GL_RED_INTEGER; out_type = GL_SHORT; return true;
      case ezGALResourceFormat::RShortNormalized: out_format = GL_RED; out_type = GL_SHORT; return true;
      // R8
      case ezGALResourceFormat::RUByte: out_format = GL_RED_INTEGER; out_type = GL_UNSIGNED_BYTE; return true;
      case ezGALResourceFormat::RUByteNormalized: out_format = GL_RED; out_type = GL_UNSIGNED_BYTE; return true;
      case ezGALResourceFormat::RByte: out_format = GL_RED_INTEGER; out_type = GL_BYTE; return true;
      case ezGALResourceFormat::RByteNormalized: out_format = GL_RED; out_type = GL_BYTE; return true;
      // single channel alpha
      case ezGALResourceFormat::AUByteNormalized: out_format = GL_RED; out_type = GL_UNSIGNED_BYTE; return true;

      // depth formats: read back via DEPTH_COMPONENT
      case ezGALResourceFormat::DFloat: out_format = GL_DEPTH_COMPONENT; out_type = GL_FLOAT; return true;
      case ezGALResourceFormat::D16: out_format = GL_DEPTH_COMPONENT; out_type = GL_UNSIGNED_SHORT; return true;
      case ezGALResourceFormat::D24S8: out_format = GL_DEPTH24_STENCIL8; out_type = GL_UNSIGNED_INT_24_8; return true;
      case ezGALResourceFormat::DFloatS8: out_format = GL_DEPTH32F_STENCIL8; out_type = GL_FLOAT_32_UNSIGNED_INT_24_8_REV; return true;

      default:
        // compressed formats have no direct client side representation
        return false;
    }
  }

  bool GetReadPixelsFormat(ezGALResourceFormat::Enum format, GLenum& out_format, GLenum& out_type)
  {
    // glReadPixels only supports a very limited subset of formats, map everything else to RGBA8 or float
    switch (format)
    {
      case ezGALResourceFormat::RGBAFloat: out_format = GL_RGBA; out_type = GL_FLOAT; return true;
      case ezGALResourceFormat::RGBAHalf: out_format = GL_RGBA; out_type = GL_HALF_FLOAT; return true;
      case ezGALResourceFormat::DFloat: out_format = GL_DEPTH_COMPONENT; out_type = GL_FLOAT; return true;
      case ezGALResourceFormat::D24S8: out_format = GL_DEPTH_STENCIL; out_type = GL_UNSIGNED_INT_24_8; return true;
      case ezGALResourceFormat::DFloatS8: out_format = GL_DEPTH_STENCIL; out_type = GL_FLOAT_32_UNSIGNED_INT_24_8_REV; return true;
      case ezGALResourceFormat::D16: out_format = GL_DEPTH_COMPONENT; out_type = GL_UNSIGNED_SHORT; return true;
      default:
        if (ezGALResourceFormat::IsDepthFormat(format))
        {
          out_format = GL_DEPTH_COMPONENT;
          out_type = GL_FLOAT;
          return true;
        }
        else
        {
          out_format = GL_RGBA;
          out_type = GL_UNSIGNED_BYTE;
          return true;
        }
    }
  }

  GLint GetTextureMinFilter(ezGALResourceFormat::Enum format, bool bUseMipMaps)
  {
    if (bUseMipMaps)
      return GL_LINEAR_MIPMAP_LINEAR;
    return GL_LINEAR;
  }
} // namespace ezConversionUtilsOpenGL
