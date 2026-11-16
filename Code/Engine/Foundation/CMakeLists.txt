ez_cmake_init()

# Get the name of this folder as the project name
get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

ez_create_target(LIBRARY ${PROJECT_NAME})

if(EZ_CMAKE_PLATFORM_WINDOWS)
  target_link_libraries(${PROJECT_NAME}
    PRIVATE

    Rpcrt4.lib
  )
endif()

ez_enable_strict_warnings(${PROJECT_NAME})

if(EZ_CMAKE_COMPILER_CLANG)
  set_source_files_properties(ThirdParty/xxHash/xxHash.c PROPERTIES COMPILE_FLAGS "-Wno-unused-function")
endif()

ez_target_link_livepp(${PROJECT_NAME})

if (EZ_CMAKE_PLATFORM_LINUX)
  CHECK_INCLUDE_FILE_CXX("uuid/uuid.h" UUID_HEADER)

  # uuid/uuid.h is only used at compile time; Foundation has a fallback implementation
  # (see Platform/Posix/UuidGenerator_Posix.h), so don't hard-fail when the dev package is missing.
  if(NOT UUID_HEADER)
    message(WARNING "Missing uuid/uuid.h. UUID generation will use the fallback implementation.")
  endif()

  find_library(UUID_LIBRARY NAMES uuid)

  if(UUID_LIBRARY)
    target_link_libraries(${PROJECT_NAME}
      PRIVATE

      ${UUID_LIBRARY}
    )
  endif()

  target_link_libraries(${PROJECT_NAME}
    PRIVATE

    dl # dlopen, dlclose, etc
  )
endif()

if(EZ_CMAKE_PLATFORM_OSX)
  find_library(CORESERVICES_LIBRARY CoreServices)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)

  mark_as_advanced(FORCE CORESERVICES_LIBRARY COREFOUNDATION_LIBRARY)

  target_link_libraries(${PROJECT_NAME}
    PRIVATE

    ${CORESERVICES_LIBRARY}
    ${COREFOUNDATION_LIBRARY}
  )
endif()

if(EZ_3RDPARTY_ENET_SUPPORT)
  target_link_libraries(${PROJECT_NAME} PUBLIC enet)
endif()

if(EZ_3RDPARTY_ZSTD_SUPPORT)
  target_link_libraries(${PROJECT_NAME} PUBLIC zstd)
endif()

if(EZ_3RDPARTY_ZLIB_SUPPORT)
  # ZLIB is required to read the apk on Android.
  target_link_libraries(${PROJECT_NAME} PUBLIC zlib)
endif()

if(EZ_3RDPARTY_GLFW_SUPPORT)
  target_compile_definitions(${PROJECT_NAME} PUBLIC BUILDSYSTEM_ENABLE_GLFW_SUPPORT)
  target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
endif()

if(EZ_3RDPARTY_TRACELOGGING_LTTNG_SUPPORT)
  target_link_libraries(${PROJECT_NAME} PUBLIC tracelogging)
endif()

if(EZ_3RDPARTY_PERFETTO_SUPPORT)
  target_link_libraries(${PROJECT_NAME} PUBLIC perfetto)
endif()

if(EZ_3RDPARTY_TRACY_SUPPORT)
  # Shipping builds don't link Tracy, see Code/ThirdParty/tracy/CMakeLists.txt
  ez_pull_config_vars()
  target_link_libraries(${PROJECT_NAME} PUBLIC $<$<NOT:$<CONFIG:${EZ_BUILDTYPENAME_RELEASE}>>:TracyClient>)
endif()
