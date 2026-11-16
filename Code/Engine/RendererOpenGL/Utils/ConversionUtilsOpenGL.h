#pragma once

#include <Foundation/Basics.h>
#include <RendererFoundation/Resources/ResourceFormats.h>
#include <glad/glad.h>

using ezGALFormatLookupEntryOpenGL = ezGALFormatLookupEntry<GLenum, 0>;
using ezGALFormatLookupTableOpenGL = ezGALFormatLookupTable<ezGALFormatLookupEntryOpenGL>;

namespace ezConversionUtilsOpenGL
{
  /// Fills the format lookup table. Must be called before any format lookup happens.
  void FillFormatLookupTable(ezGALFormatLookupTableOpenGL& ref_table);

  /// Returns the client side pixel format and type for the given resource format.
  /// \return false if the format is compressed or has no direct client side representation.
  bool GetPixelFormat(ezGALResourceFormat::Enum format, GLenum& out_format, GLenum& out_type);

  /// Returns the GL internal format for the given resource format.
  GLenum GetInternalFormat(ezGALResourceFormat::Enum format);

  /// Returns the GL format to use for glReadPixels of the given format.
  bool GetReadPixelsFormat(ezGALResourceFormat::Enum format, GLenum& out_format, GLenum& out_type);

  /// Returns the filter for glTexParameteri for magnification/minification.
  GLint GetTextureMinFilter(enum ezGALResourceFormat::Enum format, bool bUseMipMaps);
} // namespace ezConversionUtilsOpenGL
