#pragma once

#include <RendererOpenGL/RendererOpenGLDLL.h>
#include <RendererFoundation/Descriptors/Enumerations.h>
#include <RendererFoundation/RendererFoundationDLL.h>

#include <Foundation/Containers/Deque.h>
#include <Foundation/Containers/DynamicArray.h>
#include <Foundation/Time/Time.h>

#include <glad/glad.h>

class ezGALDeviceOpenGL;

/// Pool for GPU queries (timestamps via GL_TIMESTAMP query counters, occlusion queries via GL_SAMPLES_PASSED).
/// Query objects are created on demand and recycled; stale queries are reaped after a few frames.
class EZ_RENDEREROPENGL_DLL ezQueryPoolOpenGL
{
public:
  ezQueryPoolOpenGL(const ezGALDeviceOpenGL* pDevice);
  ~ezQueryPoolOpenGL();

  ezResult Initialize();
  void DeInitialize();

  void BeginFrame();
  void EndFrame();

  ezGALTimestampHandle InsertTimestamp();
  ezEnum<ezGALAsyncResult> GetTimestampResult(ezGALTimestampHandle hTimestamp, ezTime& out_result);

  ezGALOcclusionHandle BeginOcclusionQuery(ezEnum<ezGALQueryType> type);
  void EndOcclusionQuery(ezGALOcclusionHandle hOcclusion);
  ezEnum<ezGALAsyncResult> GetOcclusionQueryResult(ezGALOcclusionHandle hOcclusion, ezUInt64& out_uiResult);

private:
  struct QueryEntry
  {
    GLuint m_uiQuery = 0;
    ezUInt64 m_uiGeneration = 0;
    ezUInt64 m_uiFrame = 0;
    GLenum m_eType = 0;      ///< 0 for timestamp queries, GL_SAMPLES_PASSED / GL_ANY_SAMPLES_PASSED for occlusion.
    bool m_bInFlight = false; ///< BeginQuery issued, EndQuery pending.
    bool m_bFree = true;
    bool m_bIsTimestamp = false;
  };

  ezUInt32 AllocateQuery();
  void RecycleQuery(ezUInt32 uiIndex);
  void ReapStaleQueries();
  ezGALPoolHandle MakeHandle(ezUInt32 uiIndex, ezUInt64 uiGeneration);
  bool TryGetQuery(ezGALPoolHandle hHandle, QueryEntry*& out_pEntry, ezUInt32& out_uiIndex);

private:
  static constexpr ezUInt32 s_uiReapFrames = 8;

  const ezGALDeviceOpenGL* m_pDevice = nullptr;
  ezDynamicArray<QueryEntry> m_Queries;
  ezDeque<ezUInt32> m_FreeQueries;
  ezUInt64 m_uiFrameCounter = 0;

  // GPU -> CPU clock sync, measured at pool initialization.
  ezTime m_CpuBaseTime;
  ezUInt64 m_uiGpuBaseTimeNs = 0;
};
