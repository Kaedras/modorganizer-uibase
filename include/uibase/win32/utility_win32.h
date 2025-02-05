#pragma once

#include <QDir>
#include <QString>
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

inline std::wstring formatSystemMessage(int id)
{
  return formatSystemMessage(static_cast<DWORD>(id));
}

/**
 * throws on failure
 * @param id    the folder id
 * @param what  the name of the folder, used for logging errors only
 * @return absolute path of the given known folder id
 **/
QDLLEXPORT QDir getKnownFolder(KNOWNFOLDERID id, const QString& what = {});

// same as above, does not log failure
//
QDLLEXPORT QString getOptionalKnownFolder(KNOWNFOLDERID id);

}  // namespace MOBase