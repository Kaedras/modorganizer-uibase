#pragma once

#include "../dllimport.h"
#include <cstdint>
#include <poll.h>
#include <unistd.h>
extern "C"
{
#include <sys/pidfd.h>
}

// windows types

using WORD    = uint16_t;
using DWORD   = uint32_t;
using LPDWORD = uint32_t*;
using HANDLE  = int;
using LPCWSTR = const wchar_t*;

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

// windows function wrappers

inline int GetLastError()
{
  return errno;
}

inline void SetLastError(int error)
{
  errno = error;
}

/* Get the process ID of the calling process.  */
inline DWORD GetCurrentProcessId()
{
  return getpid();
}

/* Returns a file descriptor that refers to the process PID.  The
   close-on-exec is set on the file descriptor.  */
inline HANDLE GetCurrentProcess()
{
  return pidfd_open(getpid(), 0);
}

/* Query the process ID (PID) from process descriptor FD.  Return the PID
   or -1 in case of an error.  */
inline DWORD GetProcessId(HANDLE fd)
{
  return pidfd_getpid(fd);
}

/* Close the file descriptor FD.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
inline bool CloseHandle(HANDLE fd)
{
  return close(fd) == 0;
}

/* Close the file descriptor FD.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
inline int NtClose(HANDLE fd)
{
  return close(fd);
}

/* Poll the file descriptors. If TIMEOUT is nonzero and not -1, allow TIMEOUT
   milliseconds for an event to occur; if TIMEOUT is -1, block until an event occurs.
   Returns the number of file descriptors with events, zero if timed out,
   or -1 for errors.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
inline int WaitForSingleObject(HANDLE fd, int timeout)
{
  pollfd pfd = {fd, POLLIN, 0};
  return poll(&pfd, 1, timeout);
}

#define sprintf_s sprintf
