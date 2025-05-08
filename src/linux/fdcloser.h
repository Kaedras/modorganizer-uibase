#pragma once

#include "../dllimport.h"

#include <cstddef>

class QDLLEXPORT FdCloser
{
public:
  constexpr FdCloser() noexcept;
  constexpr FdCloser(std::nullptr_t) noexcept;
  explicit FdCloser(int fd) noexcept;

  FdCloser(FdCloser&&)      = default;
  FdCloser(const FdCloser&) = delete;

  FdCloser& operator=(FdCloser&& other) noexcept;

  ~FdCloser();

  FdCloser& operator=(int fd) noexcept;
  int operator->() const noexcept;
  int operator*() const noexcept;

  explicit operator bool() const noexcept;

  int get() const noexcept;

  void reset(int fd) noexcept;

  int release() noexcept;

  bool isValid() const noexcept;

private:
  int m_fd;
};
