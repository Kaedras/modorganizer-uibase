#include "linux/fdcloser.h"

FdCloser::FdCloser() noexcept : m_fd(-1) {}

FdCloser::FdCloser(std::nullptr_t) noexcept : m_fd(-1) {}

FdCloser::FdCloser(int fd) noexcept : m_fd(fd) {}

FdCloser& FdCloser::operator=(FdCloser&& other) noexcept
{
  if (m_fd != -1) {
    close(m_fd);
  }
  m_fd = other.m_fd;
  return *this;
}

FdCloser::~FdCloser() noexcept
{
  if (m_fd != -1) {
    close(m_fd);
  }
}

FdCloser& FdCloser::operator=(int fd) noexcept
{
  if (m_fd != -1) {
    close(m_fd);
  }
  m_fd = fd;

  return *this;
}

FdCloser::operator bool() const noexcept
{
  return isValid();
}

int FdCloser::operator->() const noexcept
{
  return m_fd;
}

int FdCloser::operator*() const noexcept
{
  return m_fd;
}

int FdCloser::get() const noexcept
{
  return m_fd;
}

void FdCloser::reset(int value) noexcept
{
  if (isValid()) {
    close(m_fd);
  }
  m_fd = value;
}

int FdCloser::release() noexcept
{
  int tmp = m_fd;
  m_fd    = -1;
  return tmp;
}

bool FdCloser::isValid() const noexcept
{
  return m_fd != -1;
}
