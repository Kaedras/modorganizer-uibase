#ifndef UIBASE_MOASSERT_INCLUDED
#define UIBASE_MOASSERT_INCLUDED

#include "log.h"

#ifdef __cpp_lib_debugging
#include <debugging>
#endif

namespace MOBase
{

template <class T>
inline void MOAssert(T&& t, const char* exp, const char* file, int line,
                     const char* func)
{
  if (!t) {
    log::error("assertion failed: {}:{} {}: '{}'", file, line, func, exp);

#ifdef __cpp_lib_debugging
    std::breakpoint_if_debugging();
#else
    if (IsDebuggerPresent()) {
      DebugBreak();
    }
#endif
  }
}

}  // namespace MOBase

#define MO_ASSERT(v) MOAssert(v, #v, __FILE__, __LINE__, Q_FUNC_INFO)

#endif  // UIBASE_MOASSERT_INCLUDED
