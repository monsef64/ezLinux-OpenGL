#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <RendererOpenGL/Shader/ShaderOpenGL.h>
#include <RendererOpenGL/Utils/MappingTablesOpenGL.h>

#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>

#include <Foundation/Strings/StringBuilder.h>
#include <Foundation/Types/ScopeExit.h>

static GLenum GALShaderStageToGLShaderType(ezGALShaderStage::Enum stage)
{
  switch (stage)
  {
    case ezGALShaderStage::VertexShader:
      return GL_VERTEX_SHADER;
    case ezGALShaderStage::HullShader:
      return GL_TESS_CONTROL_SHADER;
    case ezGALShaderStage::DomainShader:
      return GL_TESS_EVALUATION_SHADER;
    case ezGALShaderStage::GeometryShader:
      return GL_GEOMETRY_SHADER;
    case ezGALShaderStage::PixelShader:
      return GL_FRAGMENT_SHADER;
    case ezGALShaderStage::ComputeShader:
      return GL_COMPUTE_SHADER;
    default:
      return 0;
  }
}

ezGALShaderOpenGL::ezGALShaderOpenGL(const ezGALShaderCreationDescription& Description)
  : ezGALShader(Description)
{
}

ezGALShaderOpenGL::~ezGALShaderOpenGL() = default;

void ezGALShaderOpenGL::SetDebugName(ezStringView sName) const
{
  if (m_uiProgram != 0)
  {
    if (glObjectLabel != nullptr)
    {
      ezStringBuilder tmp = sName;
      glObjectLabel(GL_PROGRAM, m_uiProgram, static_cast<GLsizei>(tmp.GetElementCount()), tmp.GetData());
    }
  }
}

/* static */ ezUInt64 ezGALShaderOpenGL::MakeFlatBindingKey(ezGLBindingClass bindingClass, ezInt16 iBindGroup, ezInt16 iSlot)
{
  return (static_cast<ezUInt64>(static_cast<ezUInt8>(bindingClass)) << 40) | (static_cast<ezUInt64>(static_cast<ezUInt16>(iBindGroup)) << 16) | static_cast<ezUInt64>(static_cast<ezUInt16>(iSlot));
}

ezUInt32 ezGALShaderOpenGL::GetFlatBinding(ezGLBindingClass bindingClass, ezInt16 iBindGroup, ezInt16 iSlot) const
{
  const ezUInt32* pBinding = m_FlatBindings.GetValue(MakeFlatBindingKey(bindingClass, iBindGroup, iSlot));
  return pBinding != nullptr ? *pBinding : 0xFFFFFFFFu;
}

/// Binding classes that consume GL binding points. Textures, samplers and images all consume texture units; UBOs use GL_UNIFORM_BUFFER and SSBOs use GL_SHADER_STORAGE_BUFFER binding points.
namespace
{
  using BindingClass = ezGLBindingClass;

  struct FlatBindingEntry
  {
    BindingClass m_Class;
    ezInt16 m_iBindGroup;
    ezInt16 m_iSlot;
    ezUInt32 m_uiFlat = 0;
  };
} // namespace


// TEMP-DEBUG: dump the layout of all buffer blocks in the module. Used to diagnose SPIRV-Cross packing failures.
static void DumpBufferBlocks(spirv_cross::CompilerGLSL& ref_compiler, const char* szTag)
{
  const auto resources = ref_compiler.get_shader_resources();
  const auto dumpList = [&ref_compiler, szTag](const spirv_cross::SmallVector<spirv_cross::Resource>& list, const char* szKind)
  {
    for (const auto& resource : list)
    {
      const spirv_cross::SPIRType& blockType = ref_compiler.get_type(resource.base_type_id);
      const ezUInt32 uiMembers = static_cast<ezUInt32>(blockType.member_types.size());
      if (uiMembers == 0)
      {
        ezLog::Error("TEMP-DEBUG {0} {1} '{2}': no members", szTag, szKind, resource.name.c_str());
        continue;
      }
      ezStringBuilder sTypes;
      ezStringBuilder sOffsets;
      ezStringBuilder sFlags;
      for (ezUInt32 i = 0; i < uiMembers; ++i)
      {
        const spirv_cross::SPIRType& memberType = ref_compiler.get_type(blockType.member_types[i]);
        const uint32_t uiOffset = ref_compiler.get_member_decoration(blockType.self, i, spv::DecorationOffset);
        if (memberType.basetype == spirv_cross::SPIRType::Struct)
        {
          sTypes.AppendFormat("{0}struct", i == 0 ? "" : ",");
        }
        else if (memberType.columns > 1)
          sTypes.AppendFormat("{0}f{1}x{2}", i == 0 ? "" : ",", memberType.vecsize, memberType.columns);
        else if (memberType.vecsize > 1)
          sTypes.AppendFormat("{0}v{1}", i == 0 ? "" : ",", memberType.vecsize);
        else
        {
          switch (memberType.basetype)
          {
            case spirv_cross::SPIRType::Boolean:
              sTypes.AppendFormat("{0}bool", i == 0 ? "" : ",");
              break;
            case spirv_cross::SPIRType::Float:
              sTypes.AppendFormat("{0}float", i == 0 ? "" : ",");
              break;
            case spirv_cross::SPIRType::Int:
              sTypes.AppendFormat("{0}int", i == 0 ? "" : ",");
              break;
            case spirv_cross::SPIRType::UInt:
              sTypes.AppendFormat("{0}uint", i == 0 ? "" : ",");
              break;
            case spirv_cross::SPIRType::Half:
              sTypes.AppendFormat("{0}half", i == 0 ? "" : ",");
              break;
            default:
              sTypes.AppendFormat("{0}base{1}", i == 0 ? "" : ",", (int)memberType.basetype);
              break;
          }
        }
        if (!memberType.array.empty())
        {
          sTypes.AppendFormat("[{0}]", memberType.array[0]);
        }
        sOffsets.AppendFormat("{0}{1}", i == 0 ? "" : ",", uiOffset);
        const auto& flags = ref_compiler.get_member_decoration_bitset(blockType.self, i);
        if (flags.get(spv::DecorationRowMajor))
          sFlags.AppendFormat("{0}r{1}", sFlags.IsEmpty() ? "" : ",", i);
        if (flags.get(spv::DecorationColMajor))
          sFlags.AppendFormat("{0}c{1}", sFlags.IsEmpty() ? "" : ",", i);
      }
      ezLog::Error("TEMP-DEBUG {0} {1} '{2}' types={3}", szTag, szKind, resource.name.c_str(), sTypes);
      ezLog::Error("TEMP-DEBUG {0} {1} '{2}' offsets={3} rowmajor={4}", szTag, szKind, resource.name.c_str(), sOffsets, sFlags.IsEmpty() ? "-" : sFlags);
    }
  };
  dumpList(resources.storage_buffers, "ssbo");
  dumpList(resources.uniform_buffers, "ubo");
  dumpList(resources.push_constant_buffers, "push");
}


ezResult ezGALShaderOpenGL::InitPlatform(ezGALDevice* pDevice)
{
  m_pDevice = pDevice;
  EZ_SUCCEED_OR_RETURN(CreateBindingMapping(true));
  EZ_SUCCEED_OR_RETURN(CreateLayouts(pDevice, false));

  // The OpenGL backend consumes SPIR-V bytecode which is converted to GLSL at load time.
  // Assign flat GL binding indices for all resources program-globally so that the layout(binding = N)
  // qualifiers are unique across all stages, then cross-compile each stage.

  spirv_cross::CompilerGLSL* compilers[ezGALShaderStage::ENUM_COUNT] = {nullptr};

  // Ensure all compilers are deleted when leaving this scope.
  auto finally = ezMakeScopeExit([&]
    {
    for (auto*& p : compilers)
    {
      delete p;
      p = nullptr;
    } });

  // Parse all stages first.
  for (ezUInt32 stage = 0; stage < ezGALShaderStage::ENUM_COUNT; stage++)
  {
    if (!m_Description.HasByteCodeForStage((ezGALShaderStage::Enum)stage))
      continue;

    const ezGALShaderByteCode* pByteCode = m_Description.m_ByteCodes[stage].Borrow();
    const ezUInt32* pSpirv = reinterpret_cast<const ezUInt32*>(pByteCode->GetByteCode());
    const size_t uiSpirvCount = pByteCode->GetSize() / 4;

    try
    {
      compilers[stage] = new spirv_cross::CompilerGLSL(pSpirv, uiSpirvCount);

      // The SPIR-V uses Vulkan-style separate texture and sampler objects. GLSL has no separate
      // texture/sampler, so the aliased images and samplers must be combined into combined image samplers
      // before resource collection or compilation, otherwise the GLSL backend throws
      // "Cannot find mapping for combined sampler, was build_combined_image_samplers() used?".
      // Shaders that use texelFetch / imageLoad without a sampler need a dummy sampler first;
      // creating it must happen BEFORE build_combined_image_samplers(), which is where the
      // 'texelFetch without sampler' error is raised.
      compilers[stage]->build_dummy_sampler_for_combined_images();
      compilers[stage]->build_combined_image_samplers();
    }
    catch (const spirv_cross::CompilerError& e)
    {
      ezLog::Error("Failed to parse SPIR-V for stage {0}: {1}", ezGALShaderStage::Names[stage], e.what());
      return EZ_FAILURE;
    }
  }

  // Collect resources across all stages and assign flat binding indices.
  ezDynamicArray<FlatBindingEntry> flatEntries;
  ezUInt32 uiNextUniformBuffer = 0;
  ezUInt32 uiNextStorageBuffer = 0;
  ezUInt32 uiNextImageUnit = 0;
  ezUInt32 uiNextAtomicCounter = 0;

  auto FindOrAddEntry = [&flatEntries](BindingClass cls, ezInt16 iBindGroup, ezInt16 iSlot) -> FlatBindingEntry&
  {
    for (auto& entry : flatEntries)
    {
      if (entry.m_Class == cls && entry.m_iBindGroup == iBindGroup && entry.m_iSlot == iSlot)
      {
        return entry;
      }
    }
    FlatBindingEntry newEntry;
    newEntry.m_Class = cls;
    newEntry.m_iBindGroup = iBindGroup;
    newEntry.m_iSlot = iSlot;
    flatEntries.PushBack(newEntry);
    return flatEntries.PeekBack();
  };

  auto addResourceList = [&](spirv_cross::CompilerGLSL* pCompiler, const spirv_cross::SmallVector<spirv_cross::Resource>& list, BindingClass cls)
  {
    for (const auto& resource : list)
    {
      const ezInt32 iBindGroup = static_cast<ezInt32>(pCompiler->get_decoration(resource.id, spv::DecorationDescriptorSet));
      const ezInt32 iSlot = static_cast<ezInt32>(pCompiler->get_decoration(resource.id, spv::DecorationBinding));
      FindOrAddEntry(cls, static_cast<ezInt16>(iBindGroup), static_cast<ezInt16>(iSlot));
    }
  };

  for (ezUInt32 stage = 0; stage < ezGALShaderStage::ENUM_COUNT; stage++)
  {
    spirv_cross::CompilerGLSL* pCompiler = compilers[stage];
    if (pCompiler == nullptr)
      continue;

    const spirv_cross::ShaderResources resources = pCompiler->get_shader_resources();

    addResourceList(pCompiler, resources.uniform_buffers, BindingClass::UniformBuffer);
    addResourceList(pCompiler, resources.storage_buffers, BindingClass::StorageBuffer);
    addResourceList(pCompiler, resources.storage_images, BindingClass::ImageUnit);
    addResourceList(pCompiler, resources.sampled_images, BindingClass::ImageUnit);
    addResourceList(pCompiler, resources.separate_images, BindingClass::ImageUnit);
    addResourceList(pCompiler, resources.separate_samplers, BindingClass::ImageUnit);
    addResourceList(pCompiler, resources.subpass_inputs, BindingClass::ImageUnit);
    addResourceList(pCompiler, resources.atomic_counters, BindingClass::AtomicCounter);

    // Push constants are emitted as a uniform buffer with a reserved GAL slot that maps to the encoder's push constant buffer.
    if (!resources.push_constant_buffers.empty())
    {
      FindOrAddEntry(BindingClass::UniformBuffer, -1, 0);
    }
  }

  for (auto& entry : flatEntries)
  {
    switch (entry.m_Class)
    {
      case BindingClass::UniformBuffer:
        entry.m_uiFlat = uiNextUniformBuffer++;
        break;
      case BindingClass::StorageBuffer:
        entry.m_uiFlat = uiNextStorageBuffer++;
        break;
      case BindingClass::ImageUnit:
        entry.m_uiFlat = uiNextImageUnit++;
        break;
      case BindingClass::AtomicCounter:
        entry.m_uiFlat = uiNextAtomicCounter++;
        break;
    }
  }

  GLint iMaxTextureImageUnits = 0;
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &iMaxTextureImageUnits);
  if (static_cast<GLint>(uiNextImageUnit) > iMaxTextureImageUnits)
  {
    ezLog::Error("Shader requires {0} texture units, but only {1} are supported.", uiNextImageUnit, iMaxTextureImageUnits);
    return EZ_FAILURE;
  }

  GLint iMaxUniformBlocks = 0;
  glGetIntegerv(GL_MAX_COMBINED_UNIFORM_BLOCKS, &iMaxUniformBlocks);
  if (static_cast<GLint>(uiNextUniformBuffer) > iMaxUniformBlocks)
  {
    ezLog::Error("Shader requires {0} uniform buffers, but only {1} are supported.", uiNextUniformBuffer, iMaxUniformBlocks);
    return EZ_FAILURE;
  }

  GLint iMaxStorageBlocks = 0;
  glGetIntegerv(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS, &iMaxStorageBlocks);
  if (static_cast<GLint>(uiNextStorageBuffer) > iMaxStorageBlocks)
  {
    ezLog::Error("Shader requires {0} shader storage blocks, but only {1} are supported.", uiNextStorageBuffer, iMaxStorageBlocks);
    return EZ_FAILURE;
  }

  // Record the flat binding map and remap decorations per stage.
  m_FlatBindings.Clear();
  for (const auto& entry : flatEntries)
  {
    m_FlatBindings.Insert(MakeFlatBindingKey(entry.m_Class, entry.m_iBindGroup, entry.m_iSlot), entry.m_uiFlat);
  }

  auto RemapResourceList = [&](spirv_cross::CompilerGLSL* pCompiler, const spirv_cross::SmallVector<spirv_cross::Resource>& list, BindingClass cls)
  {
    for (const auto& resource : list)
    {
      const ezInt32 iBindGroup = static_cast<ezInt32>(pCompiler->get_decoration(resource.id, spv::DecorationDescriptorSet));
      const ezInt32 iSlot = static_cast<ezInt32>(pCompiler->get_decoration(resource.id, spv::DecorationBinding));
      for (const auto& entry : flatEntries)
      {
        if (entry.m_Class == cls && entry.m_iBindGroup == static_cast<ezInt16>(iBindGroup) && entry.m_iSlot == static_cast<ezInt16>(iSlot))
        {
          pCompiler->set_decoration(resource.id, spv::DecorationBinding, entry.m_uiFlat);
          break;
        }
      }
    }
  };

  ezStringBuilder stageGLSL[ezGALShaderStage::ENUM_COUNT];

  // Desktop GLSL cannot express blocks whose members use scalar packing with arbitrary 4 byte alignment (e.g. material structured buffers with ez's relaxed layout),
  // even with enhanced layouts. Retrying the failed stage with Vulkan semantics lets SPIRV-Cross emit scalar block layout (GL_EXT_scalar_block_layout) instead.
  // The emitted 'set = N' qualifiers are meaningless in desktop GL and are stripped again below.
  auto StripSetQualifiers = [](ezStringBuilder& ref_sGLSL)
  {
    ezStringBuilder sOut;
    const ezUInt32 uiSize = ref_sGLSL.GetElementCount();
    sOut.Reserve(uiSize);
    ezStringView sRest = ref_sGLSL.GetView();
    while (true)
    {
      const char* szFound = sRest.FindSubString("set = ");
      if (szFound == nullptr)
      {
        sOut.Append(sRest);
        break;
      }
      const size_t uiPrefix = static_cast<size_t>(szFound - sRest.GetStartPointer());
      sOut.Append(ezStringView(sRest.GetStartPointer(), szFound));

      const char* szNum = szFound + 6;
      while (*szNum >= '0' && *szNum <= '9')
        ++szNum;
      if (*szNum == ',') // 'set = N, ' is always the first qualifier in a layout() list
      {
        ++szNum;
        if (*szNum == ' ')
          ++szNum;
        sRest = ezStringView(szNum, sRest.GetEndPointer());
      }
      else
      {
        // Not in the expected form; keep the text as-is.
        sOut.Append(ezStringView(szFound, szNum));
        sRest = ezStringView(szNum, sRest.GetEndPointer());
      }
    }
    ref_sGLSL = sOut;
  };

  auto ConvertStage = [&](spirv_cross::CompilerGLSL* pCompiler, ezUInt32 uiStage, bool bVulkanSemantics) -> ezResult
  {
    spirv_cross::CompilerGLSL::Options options;
    options.version = 450;
    options.es = false;
    options.vulkan_semantics = bVulkanSemantics;
    options.emit_push_constant_as_uniform_buffer = true;
    options.enable_420pack_extension = false;
    pCompiler->set_common_options(options);

    const spirv_cross::ShaderResources resources = pCompiler->get_shader_resources();
    RemapResourceList(pCompiler, resources.uniform_buffers, BindingClass::UniformBuffer);
    RemapResourceList(pCompiler, resources.storage_buffers, BindingClass::StorageBuffer);
    RemapResourceList(pCompiler, resources.storage_images, BindingClass::ImageUnit);
    RemapResourceList(pCompiler, resources.sampled_images, BindingClass::ImageUnit);
    RemapResourceList(pCompiler, resources.separate_images, BindingClass::ImageUnit);
    RemapResourceList(pCompiler, resources.separate_samplers, BindingClass::ImageUnit);
    RemapResourceList(pCompiler, resources.subpass_inputs, BindingClass::ImageUnit);
    RemapResourceList(pCompiler, resources.atomic_counters, BindingClass::AtomicCounter);

    // Give the push constant block a flat binding so the encoder can bind its dedicated buffer to it.
    for (const auto& resource : resources.push_constant_buffers)
    {
      for (const auto& entry : flatEntries)
      {
        if (entry.m_Class == BindingClass::UniformBuffer && entry.m_iBindGroup == -1)
        {
          pCompiler->set_decoration(resource.id, spv::DecorationBinding, entry.m_uiFlat);
          break;
        }
      }
    }

    // Combined image samplers take the binding of the image they are built from.
    for (const auto& combined : pCompiler->get_combined_image_samplers())
    {
      if (pCompiler->has_decoration(combined.image_id, spv::DecorationBinding))
      {
        const ezUInt32 uiBinding = pCompiler->get_decoration(combined.image_id, spv::DecorationBinding);
        pCompiler->set_decoration(combined.combined_id, spv::DecorationBinding, uiBinding);
      }
    }

    stageGLSL[uiStage].Set(pCompiler->compile().c_str());
    if (bVulkanSemantics)
    {
      StripSetQualifiers(stageGLSL[uiStage]);
    }
    return EZ_SUCCESS;
  };

  // Set GLSL options and remap each stage.
  for (ezUInt32 stage = 0; stage < ezGALShaderStage::ENUM_COUNT; stage++)
  {
    spirv_cross::CompilerGLSL* pCompiler = compilers[stage];
    if (pCompiler == nullptr)
      continue;

    try
    {
      ConvertStage(pCompiler, stage, false).IgnoreResult();
    }
    catch (const spirv_cross::CompilerError& e)
    {
      // Retry the stage with Vulkan semantics so scalar-layout blocks can be emitted as scalar block layout.
      ezLog::Info("Failed to convert stage {0} to desktop GLSL ({1}); retrying with scalar block layout.", ezGALShaderStage::Names[stage], e.what());
      DumpBufferBlocks(*pCompiler, ezGALShaderStage::Names[stage]);
      const ezGALShaderByteCode* pByteCode = m_Description.m_ByteCodes[stage].Borrow();
      const ezUInt32* pSpirv = reinterpret_cast<const ezUInt32*>(pByteCode->GetByteCode());
      const size_t uiSpirvCount = pByteCode->GetSize() / 4;
      try
      {
        spirv_cross::CompilerGLSL retryCompiler(pSpirv, uiSpirvCount);
        retryCompiler.build_dummy_sampler_for_combined_images();
        retryCompiler.build_combined_image_samplers();
        ConvertStage(&retryCompiler, stage, true).IgnoreResult();
      }
      catch (const spirv_cross::CompilerError& e2)
      {
        ezLog::Error("Failed to convert SPIR-V to GLSL for stage {0}: {1}", ezGALShaderStage::Names[stage], e2.what());
        return EZ_FAILURE;
      }
    }
  }

  // Compile GLSL stages and link the program.
  GLuint program = glCreateProgram();
  if (program == 0)
  {
    ezLog::Error("Failed to create GL program");
    return EZ_FAILURE;
  }

  GLuint stageShaders[ezGALShaderStage::ENUM_COUNT] = {0};

  auto cleanupShaders = ezMakeScopeExit([&]
    {
    for (auto& s : stageShaders)
    {
      if (s != 0)
      {
        glDeleteShader(s);
        s = 0;
      }
    } });

  for (ezUInt32 stage = 0; stage < ezGALShaderStage::ENUM_COUNT; stage++)
  {
    if (stageGLSL[stage].IsEmpty())
      continue;

    const GLenum glStage = GALShaderStageToGLShaderType((ezGALShaderStage::Enum)stage);
    GLuint shader = glCreateShader(glStage);
    if (shader == 0)
    {
      ezLog::Error("Failed to create GL shader for stage {0}", ezGALShaderStage::Names[stage]);
      glDeleteProgram(program);
      return EZ_FAILURE;
    }

    const char* szSource = stageGLSL[stage];
    glShaderSource(shader, 1, &szSource, nullptr);
    glCompileShader(shader);

    GLint iCompiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &iCompiled);
    if (iCompiled != GL_TRUE)
    {
      GLint iLogLength = 0;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &iLogLength);
      char log[4096] = {0};
      glGetShaderInfoLog(shader, sizeof(log) - 1, nullptr, log);
      ezLog::Error("Failed to compile GLSL for stage {0}:\n{1}\n{2}", ezGALShaderStage::Names[stage], log, szSource);
      glDeleteShader(shader);
      glDeleteProgram(program);
      return EZ_FAILURE;
    }

    glAttachShader(program, shader);
    stageShaders[stage] = shader;
  }

  glLinkProgram(program);

  GLint iLinked = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &iLinked);
  if (iLinked != GL_TRUE)
  {
    GLint iLogLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &iLogLength);
    char log[4096] = {0};
    glGetProgramInfoLog(program, sizeof(log) - 1, nullptr, log);
    EZ_IGNORE_UNUSED(iLogLength);
    ezLog::Error("Failed to link GL program:\n{0}", log);
    glDeleteProgram(program);
    return EZ_FAILURE;
  }

  m_uiProgram = program;
  return EZ_SUCCESS;
}

ezResult ezGALShaderOpenGL::DeInitPlatform(ezGALDevice* pDevice)
{
  DestroyBindingMapping();
  DestroyLayouts(pDevice);

  if (m_uiProgram != 0)
  {
    glDeleteProgram(m_uiProgram);
    m_uiProgram = 0;
  }
  m_FlatBindings.Clear();
  return EZ_SUCCESS;
}
