#pragma once

#include "../dllimport.h"
#include <string>

namespace MOBase
{

using HANDLE                              = pid_t;
using DWORD                               = int;
using SYSTEMTIME                          = timespec;
static constexpr int INVALID_HANDLE_VALUE = 0;

namespace details
{
  using HandlePtr = pid_t;
}

QDLLEXPORT std::string formatSystemMessage(int id);

}  // namespace MOBase