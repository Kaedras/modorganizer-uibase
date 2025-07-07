#pragma once

#include "../dllimport.h"

#include <cstddef>

class QDLLEXPORT FdCloser
{
public:
  FdCloser() noexcept;
  FdCloser(std::nullptr_t) noexcept;
  FdCloser(int fd) noexcept;

  FdCloser(FdCloser&&) noexcept = default;
  FdCloser(const FdCloser&)     = delete;

  ~FdCloser() noexcept;

  FdCloser& operator=(FdCloser&& other) noexcept;
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
