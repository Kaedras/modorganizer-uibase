#include "linux/fdcloser.h"

FdCloser::FdCloser() : m_fd(-1) {}

FdCloser::FdCloser(int fd) : m_fd(fd) {}

FdCloser::~FdCloser()
{
  if (m_fd != -1) {
    close(m_fd);
  }
}

FdCloser& FdCloser::operator=(int fd)
{
  if (m_fd != -1) {
    close(m_fd);
  }
  m_fd = fd;

  return *this;
}

FdCloser::operator bool() const noexcept
{
  return m_fd != -1;
}

int FdCloser::get() const
{
  return m_fd;
}

void FdCloser::reset(int value)
{
  m_fd = value;
}

int FdCloser::release()
{
  int tmp = m_fd;
  m_fd    = -1;
  return tmp;
}

bool FdCloser::isValid() const
{
  return m_fd != -1;
}
