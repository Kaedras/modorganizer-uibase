#pragma once

#include "../dllimport.h"
#include <string>

namespace MOBase
{

using DWORD      = uint32_t;
using SYSTEMTIME = timespec;
using HANDLE     = int;

static inline constexpr int INVALID_HANDLE_VALUE = -1;
QDLLEXPORT std::string formatSystemMessage(int id);

#define ERROR_SUCCESS EXIT_SUCCESS
#define ERROR_FILE_NOT_FOUND ENOENT
#define ERROR_PATH_NOT_FOUND ENOENT
#define ERROR_ACCESS_DENIED EACCES
#define ERROR_BAD_ARGUMENTS EINVAL

inline int GetLastError()
{
  return errno;
}

#define sprintf_s sprintf

namespace details
{
  class PidfdCloser
  {
  public:
    explicit PidfdCloser(int pidfd) : m_pidfd(pidfd) {}
    PidfdCloser() = default;

    PidfdCloser& operator=(int pidfd)
    {
      if (m_pidfd != -1) {
        close(m_pidfd);
      }
      m_pidfd = pidfd;

      return *this;
    }

    ~PidfdCloser()
    {
      if (m_pidfd != -1) {
        close(m_pidfd);
      }
    }

    int get() const { return m_pidfd; }
    void reset(int value) { m_pidfd = value; }

    int release()
    {
      int tmp = m_pidfd;
      m_pidfd = -1;
      return tmp;
    }

    bool isValid() const { return m_pidfd != -1; }

  private:
    int m_pidfd = -1;
  };
  using HandlePtr = PidfdCloser;
}  // namespace details
}  // namespace MOBase