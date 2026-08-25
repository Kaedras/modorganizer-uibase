#pragma once

#include "../dllimport.h"

#include <cstddef>

/**
 * @brief RAII wrapper for file descriptors that automatically closes them on
 *
 * destruction
 */
class QDLLEXPORT FdCloser
{
public:
  FdCloser() noexcept;
  FdCloser(std::nullptr_t) noexcept;
  FdCloser(int fd) noexcept;

  FdCloser(FdCloser&) = delete;
  FdCloser(FdCloser&&) noexcept;

  ~FdCloser() noexcept;

  FdCloser& operator=(FdCloser&) = delete;
  FdCloser& operator=(FdCloser&&) noexcept;
  FdCloser& operator=(int fd) noexcept;
  int operator->() const noexcept;
  int operator*() const noexcept;

  operator bool() const noexcept;

  int get() const noexcept;
  void reset(int fd) noexcept;
  int release() noexcept;
  bool isValid() const noexcept;

private:
  int m_fd;
};
