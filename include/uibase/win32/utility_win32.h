#pragma once

#include <ShlObj.h>
#include <Windows.h>

#include "../dllimport.h"

namespace MOBase
{

QDLLEXPORT std::wstring formatSystemMessage(DWORD id);
QDLLEXPORT std::wstring formatNtMessage(NTSTATUS s);

inline std::wstring formatSystemMessage(HRESULT hr)
{
  return formatSystemMessage(static_cast<DWORD>(hr));
}

}  // namespace MOBase