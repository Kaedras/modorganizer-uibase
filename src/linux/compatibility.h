#pragma once

#include "../dllimport.h"
#include <cstdint>
#include <unistd.h>

// windows types

using WORD       = uint16_t;
using DWORD      = uint32_t;
using LPDWORD    = uint32_t*;
using HANDLE     = int;
using LPCWSTR    = const wchar_t*;

// https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-dtyp/2fefe8dd-ab48-4e33-a7d5-7171455a9289
typedef struct _SYSTEMTIME {
  WORD wYear;
  WORD wMonth;
  WORD wDayOfWeek;
  WORD wDay;
  WORD wHour;
  WORD wMinute;
  WORD wSecond;
  WORD wMilliseconds;
} SYSTEMTIME,
 *PSYSTEMTIME;

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

// compatibility class for HandlePtr

class QDLLEXPORT FdCloser
{
public:
  FdCloser() : m_fd(-1) {}
  FdCloser(int fd) : m_fd(fd) {}

  ~FdCloser()
  {
    if (m_fd != -1) {
      close(m_fd);
    }
  }

  FdCloser& operator=(int fd)
  {
    if (m_fd != -1) {
      close(m_fd);
    }
    m_fd = fd;

    return *this;
  }

  operator bool() const noexcept { return m_fd != -1; }

  int get() const { return m_fd; }
  void reset(int value) { m_fd = value; }

  int release()
  {
    int tmp = m_fd;
    m_fd    = -1;
    return tmp;
  }

  bool isValid() const { return m_fd != -1; }

private:
  int m_fd;
};