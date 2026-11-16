#include <RendererOpenGL/RendererOpenGLPCH.h>

#include <RendererOpenGL/Pools/QueryPoolOpenGL.h>
#include <RendererOpenGL/Device/DeviceOpenGL.h>

#include <Foundation/Time/Time.h>

ezQueryPoolOpenGL::ezQueryPoolOpenGL(const ezGALDeviceOpenGL* pDevice)
  : m_pDevice(pDevice)
{
}

ezQueryPoolOpenGL::~ezQueryPoolOpenGL() = default;

ezResult ezQueryPoolOpenGL::Initialize()
{
  // Measure the GPU -> CPU clock offset once. Deltas measured on the GPU clock are exact;
  // the base only shifts the absolute time so both clocks agree at initialization time.
  GLint64 iGpuTime = 0;
  glGetInteger64v(GL_TIMESTAMP, &iGpuTime);
  m_uiGpuBaseTimeNs = static_cast<ezUInt64>(iGpuTime);
  m_CpuBaseTime = ezTime::Now();
  return EZ_SUCCESS;
}

void ezQueryPoolOpenGL::DeInitialize()
{
  for (auto& query : m_Queries)
  {
    if (query.m_uiQuery != 0)
    {
      glDeleteQueries(1, &query.m_uiQuery);
      query.m_uiQuery = 0;
    }
  }
  m_Queries.Clear();
  m_FreeQueries.Clear();
}

void ezQueryPoolOpenGL::BeginFrame()
{
  ++m_uiFrameCounter;
  ReapStaleQueries();
}

void ezQueryPoolOpenGL::EndFrame()
{
}

ezUInt32 ezQueryPoolOpenGL::AllocateQuery()
{
  if (!m_FreeQueries.IsEmpty())
  {
    const ezUInt32 uiIndex = m_FreeQueries.PeekFront();
    m_FreeQueries.PopFront();
    return uiIndex;
  }

  GLuint uiQuery = 0;
  glGenQueries(1, &uiQuery);
  if (uiQuery == 0)
  {
    return ezInvalidIndex;
  }

  QueryEntry entry;
  entry.m_uiQuery = uiQuery;
  entry.m_uiGeneration = 1;
  entry.m_bFree = false;
  m_Queries.PushBack(entry);
  return m_Queries.GetCount() - 1;
}

void ezQueryPoolOpenGL::RecycleQuery(ezUInt32 uiIndex)
{
  QueryEntry& entry = m_Queries[uiIndex];
  entry.m_bFree = true;
  entry.m_bInFlight = false;
  entry.m_eType = 0;
  entry.m_bIsTimestamp = false;
  ++entry.m_uiGeneration;
  m_FreeQueries.PushBack(uiIndex);
}

void ezQueryPoolOpenGL::ReapStaleQueries()
{
  // Delete queries whose results were never retrieved after a few frames.
  for (ezUInt32 i = 0; i < m_Queries.GetCount(); i++)
  {
    QueryEntry& entry = m_Queries[i];
    if (entry.m_bFree || entry.m_bInFlight)
      continue;

    if (m_uiFrameCounter > entry.m_uiFrame + s_uiReapFrames)
    {
      if (entry.m_uiQuery != 0)
      {
        glDeleteQueries(1, &entry.m_uiQuery);
        entry.m_uiQuery = 0;
      }
      RecycleQuery(i);
    }
  }
}

ezGALPoolHandle ezQueryPoolOpenGL::MakeHandle(ezUInt32 uiIndex, ezUInt64 uiGeneration)
{
  ezGALPoolHandle handle;
  handle.m_InstanceIndex = static_cast<ezGAL::ez20_44Id::StorageType>(uiIndex);
  handle.m_Generation = static_cast<ezGAL::ez20_44Id::StorageType>(uiGeneration & ((ezUInt64(1) << 44) - 1));
  return handle;
}

bool ezQueryPoolOpenGL::TryGetQuery(ezGALPoolHandle hHandle, QueryEntry*& out_pEntry, ezUInt32& out_uiIndex)
{
  out_pEntry = nullptr;
  out_uiIndex = ezInvalidIndex;

  const ezUInt32 uiIndex = static_cast<ezUInt32>(hHandle.m_InstanceIndex);
  if (uiIndex >= m_Queries.GetCount())
    return false;

  QueryEntry& entry = m_Queries[uiIndex];
  if (entry.m_bFree || entry.m_uiGeneration != hHandle.m_Generation)
    return false;

  out_pEntry = &entry;
  out_uiIndex = uiIndex;
  return true;
}

ezGALTimestampHandle ezQueryPoolOpenGL::InsertTimestamp()
{
  const ezUInt32 uiIndex = AllocateQuery();
  if (uiIndex == ezInvalidIndex)
    return {};

  QueryEntry& entry = m_Queries[uiIndex];
  entry.m_uiFrame = m_uiFrameCounter;
  entry.m_bFree = false;
  entry.m_bIsTimestamp = true;
  entry.m_eType = 0;

  glQueryCounter(entry.m_uiQuery, GL_TIMESTAMP);
  return ezGALTimestampHandle(MakeHandle(uiIndex, entry.m_uiGeneration));
}

ezEnum<ezGALAsyncResult> ezQueryPoolOpenGL::GetTimestampResult(ezGALTimestampHandle hTimestamp, ezTime& out_result)
{
  QueryEntry* pEntry = nullptr;
  ezUInt32 uiIndex = ezInvalidIndex;
  if (!TryGetQuery(hTimestamp, pEntry, uiIndex))
  {
    out_result = ezTime();
    return ezGALAsyncResult::Expired;
  }

  GLuint64 uiAvailable = 0;
  glGetQueryObjectui64v(pEntry->m_uiQuery, GL_QUERY_RESULT_AVAILABLE, &uiAvailable);
  if (uiAvailable == GL_FALSE)
    return ezGALAsyncResult::Pending;

  GLuint64 uiGpuTimeNs = 0;
  glGetQueryObjectui64v(pEntry->m_uiQuery, GL_QUERY_RESULT, &uiGpuTimeNs);

  const ezUInt64 uiDeltaNs = uiGpuTimeNs >= m_uiGpuBaseTimeNs ? uiGpuTimeNs - m_uiGpuBaseTimeNs : 0;
  out_result = m_CpuBaseTime + ezTime::MakeFromNanoseconds(static_cast<double>(uiDeltaNs));

  RecycleQuery(uiIndex);
  return ezGALAsyncResult::Ready;
}

ezGALOcclusionHandle ezQueryPoolOpenGL::BeginOcclusionQuery(ezEnum<ezGALQueryType> type)
{
  const ezUInt32 uiIndex = AllocateQuery();
  if (uiIndex == ezInvalidIndex)
    return {};

  QueryEntry& entry = m_Queries[uiIndex];
  entry.m_uiFrame = m_uiFrameCounter;
  entry.m_bFree = false;
  entry.m_bIsTimestamp = false;
  entry.m_bInFlight = true;
  entry.m_eType = type == ezGALQueryType::AnySamplesPassed ? GL_ANY_SAMPLES_PASSED : GL_SAMPLES_PASSED;

  glBeginQuery(entry.m_eType, entry.m_uiQuery);
  return ezGALOcclusionHandle(MakeHandle(uiIndex, entry.m_uiGeneration));
}

void ezQueryPoolOpenGL::EndOcclusionQuery(ezGALOcclusionHandle hOcclusion)
{
  QueryEntry* pEntry = nullptr;
  ezUInt32 uiIndex = ezInvalidIndex;
  if (!TryGetQuery(hOcclusion, pEntry, uiIndex))
    return;

  if (pEntry->m_bInFlight)
  {
    glEndQuery(pEntry->m_eType);
    pEntry->m_bInFlight = false;
  }
}

ezEnum<ezGALAsyncResult> ezQueryPoolOpenGL::GetOcclusionQueryResult(ezGALOcclusionHandle hOcclusion, ezUInt64& out_uiResult)
{
  QueryEntry* pEntry = nullptr;
  ezUInt32 uiIndex = ezInvalidIndex;
  if (!TryGetQuery(hOcclusion, pEntry, uiIndex))
    return ezGALAsyncResult::Expired;

  GLuint64 uiAvailable = 0;
  glGetQueryObjectui64v(pEntry->m_uiQuery, GL_QUERY_RESULT_AVAILABLE, &uiAvailable);
  if (uiAvailable == GL_FALSE)
    return ezGALAsyncResult::Pending;

  GLuint64 uiResult = 0;
  glGetQueryObjectui64v(pEntry->m_uiQuery, GL_QUERY_RESULT, &uiResult);
  out_uiResult = uiResult;

  RecycleQuery(uiIndex);
  return ezGALAsyncResult::Ready;
}
