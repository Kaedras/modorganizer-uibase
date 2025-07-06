#ifndef UIBASE_MOASSERT_INCLUDED
#define UIBASE_MOASSERT_INCLUDED

#include "log.h"

#ifndef _MSC_VER
#define __FUNCSIG__ __PRETTY_FUNCTION__
#ifdef __cpp_lib_debugging
// These functions are part of C++26
#include <debugging>
inline bool IsDebuggerPresent()
{
  return std::is_debugger_present();
}
inline void DebugBreak()
{
  std::breakpoint();
}
#else
#include <csignal>
#include <fstream>

// Detect if the application is running inside a debugger.
// Source: https://stackoverflow.com/a/69842462
inline bool IsDebuggerPresent()
{
  std::ifstream sf("/proc/self/status");
  std::string s;
  while (sf >> s) {
    if (s == "TracerPid:") {
      int pid;
      sf >> pid;
      return pid != 0;
    }
    std::getline(sf, s);
  }

  return false;
}
inline void DebugBreak()
{
  raise(SIGTRAP);
}
#endif
#endif  // _MSC_VER

namespace MOBase
{

template <class T>
inline void MOAssert(T&& t, const char* exp, const char* file, int line,
                     const char* func)
{
  if (!t) {
    log::error("assertion failed: {}:{} {}: '{}'", file, line, func, exp);

    if (IsDebuggerPresent()) {
      DebugBreak();
    }
  }
}

}  // namespace MOBase

#define MO_ASSERT(v) MOAssert(v, #v, __FILE__, __LINE__, __FUNCSIG__)

#endif  // UIBASE_MOASSERT_INCLUDED
