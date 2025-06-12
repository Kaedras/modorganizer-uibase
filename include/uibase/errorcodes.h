#ifndef UIBASE_ERRORCODES_H
#define UIBASE_ERRORCODES_H

#include "dllimport.h"
#ifndef __unix__
#include <Windows.h>
#endif

namespace MOBase
{

#ifdef __unix__
QDLLEXPORT const char* errorCodeName(int code);
#else
QDLLEXPORT const wchar_t* errorCodeName(DWORD code);
#endif

}  // namespace MOBase

#endif  // UIBASE_ERRORCODES_H
