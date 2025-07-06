#pragma once

#include "../dllimport.h"
#include <cstdint>
#include <unistd.h>

// windows types

using DWORD      = uint32_t;
using SYSTEMTIME = timespec;
using HANDLE     = int;
using LPCWSTR    = const wchar_t*;

static inline constexpr int INVALID_HANDLE_VALUE = -1;

// error codes

#define ERROR_SUCCESS EXIT_SUCCESS
#define ERROR_FILE_NOT_FOUND ENOENT
#define ERROR_PATH_NOT_FOUND ENOENT
#define ERROR_ACCESS_DENIED EACCES
#define ERROR_BAD_ARGUMENTS EINVAL
#define ERROR_CANCELLED ECANCELED

// functions

inline int GetLastError()
{
  return errno;
}

#define sprintf_s sprintf
