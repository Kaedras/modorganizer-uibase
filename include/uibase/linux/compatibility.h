#pragma once

#include <cerrno>
#include <cstdint>
#include <poll.h>
#include <unistd.h>
#include <utility>
extern "C"
{
#include <sys/pidfd.h>
}

#define __FUNCSIG__ __PRETTY_FUNCTION__

// constants

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

static constexpr uint32_t WAIT_ABANDONED = 0x00000080L;
static constexpr uint32_t WAIT_OBJECT_0  = 0x00000000L;
static constexpr uint32_t WAIT_TIMEOUT   = 0x00000102L;
static constexpr uint32_t WAIT_FAILED    = 0xFFFFFFFF;

// windows types

using BYTE             = uint8_t;
using UINT             = unsigned int;
using WORD             = uint16_t;
using DWORD            = uint32_t;
using LPDWORD          = uint32_t*;
using HANDLE           = int;
using HKEY             = int;
using LPCWSTR          = const wchar_t*;
using WCHAR            = wchar_t;
using REFKNOWNFOLDERID = int;

// https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-dtyp/2fefe8dd-ab48-4e33-a7d5-7171455a9289
typedef struct _SYSTEMTIME
{
  WORD wYear;
  WORD wMonth;
  WORD wDayOfWeek;
  WORD wDay;
  WORD wHour;
  WORD wMinute;
  WORD wSecond;
  WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME;

// https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-filetime
typedef struct _FILETIME
{
  DWORD dwLowDateTime;
  DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

static inline constexpr int INVALID_HANDLE_VALUE = -1;

// error codes

#define ERROR_SUCCESS EXIT_SUCCESS
#define ERROR_FILE_NOT_FOUND ENOENT
#define ERROR_PATH_NOT_FOUND ENOENT
#define ERROR_ACCESS_DENIED EACCES
#define ERROR_BAD_ARGUMENTS EINVAL
#define ERROR_CANCELLED ECANCELED

// windows functions

inline int GetLastError()
{
  return errno;
}

inline void SetLastError(int error)
{
  errno = error;
}

/* Get the process ID of the calling process. */
inline DWORD GetCurrentProcessId()
{
  return getpid();
}

/* Returns a file descriptor that refers to the process PID.  The
   close-on-exec is
 * set on the file descriptor. */
inline HANDLE GetCurrentProcess()
{
  return pidfd_open(getpid(), 0);
}

/* Query the process ID (PID) from process descriptor FD.  Return the PID
   or -1 in
 * case of an error. */
inline DWORD GetProcessId(HANDLE fd)
{
  return pidfd_getpid(fd);
}

/* Close the file descriptor FD. */
inline bool CloseHandle(HANDLE fd)
{
  return close(fd) == 0;
}

/* Close the file descriptor FD. */
inline int NtClose(HANDLE fd)
{
  return close(fd);
}

inline uint32_t WaitForSingleObject(HANDLE fd, int timeout)
{
  pollfd pfd = {fd, POLLIN, 0};
  int result = poll(&pfd, 1, timeout);
  if (result == 1) {
    return WAIT_OBJECT_0;
  }
  if (result == 0) {
    return WAIT_TIMEOUT;
  }
  return WAIT_FAILED;
}

template <typename... Params>
int sprintf_s(char* buffer, const char* format, Params&&... params)
{
  return sprintf(buffer, format, std::forward<Params>(params)...);
}

#ifdef __cpp_lib_debugging
// These functions are part of C++26
#include <debugging>
inline bool IsDebuggerPresent()
{
  return std::is_debugger_present();
}
inline void DebugBreak()
{
  std::breakpoint();
}
#else
#include <csignal>
#include <fstream>

// Detect if the application is running inside a debugger.
// Source: https://stackoverflow.com/a/69842462
inline bool IsDebuggerPresent()
{
  std::ifstream sf("/proc/self/status");
  std::string s;
  while (sf >> s) {
    if (s == "TracerPid:") {
      int pid;
      sf >> pid;
      return pid != 0;
    }
    std::getline(sf, s);
  }

  return false;
}
inline void DebugBreak()
{
  raise(SIGTRAP);
}
#endif
