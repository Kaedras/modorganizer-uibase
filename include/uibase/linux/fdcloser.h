#pragma once

class QDLLEXPORT FdCloser
{
public:
  FdCloser();
  FdCloser(int fd);

  ~FdCloser();

  FdCloser& operator=(int fd);

  operator bool() const noexcept;

  int get() const;
  void reset(int value);

  int release();

  bool isValid() const;

private:
  int m_fd;
};
