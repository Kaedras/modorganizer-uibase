#pragma once

#include <ShlObj.h>
#include <Windows.h>

#include "../dllimport.h"

namespace MOBase
{
namespace details
{
  // used by HandlePtr, calls CloseHandle() as the deleter
  //
  struct HandleCloser
  {
    using pointer = HANDLE;

    void operator()(HANDLE h)
    {
      if (h != INVALID_HANDLE_VALUE) {
        ::CloseHandle(h);
      }
    }
  };

  using HandlePtr = std::unique_ptr<HANDLE, HandleCloser>;
}  // namespace details

/**
 * throws on failure
 * @param id    the folder id
 * @param what  the name of the
 * folder, used for logging errors only
 * @return absolute path of the given known
 * folder id
 **/
QDLLEXPORT QDir getKnownFolder(KNOWNFOLDERID id, const QString& what = {});

// same as above, does not log failure
//
QDLLEXPORT QString getOptionalKnownFolder(KNOWNFOLDERID id);

QDLLEXPORT std::wstring formatSystemMessage(DWORD id);
QDLLEXPORT std::wstring formatNtMessage(NTSTATUS s);

inline std::wstring formatSystemMessage(HRESULT hr)
{
  return formatSystemMessage(static_cast<DWORD>(hr));
}

}  // namespace MOBase