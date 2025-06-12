#ifndef UTILITY_WIN32_H
#define UTILITY_WIN32_H

#include <memory>

namespace MOBase::shell::details
{

// used by HandlePtr, calls CloseHandle() as the deleter
//
struct HandleCloser
{
  using pointer = HANDLE;

  void operator()(HANDLE h)
  {
    if (h != INVALID_HANDLE_VALUE) {
      ::CloseHandle(h);
    }
  }
};

using HandlePtr = std::unique_ptr<HANDLE, HandleCloser>;

}  // namespace MOBase::shell::details

#endif  // UTILITY_WIN32_H
