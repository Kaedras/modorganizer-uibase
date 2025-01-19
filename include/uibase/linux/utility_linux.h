#pragma once

#include "../dllimport.h"
#include <string>

namespace MOBase
{

using DWORD      = uint32_t;
using SYSTEMTIME = timespec;

namespace details
{
  using HandlePtr = pid_t;
}

QDLLEXPORT std::string formatSystemMessage(int id);

}  // namespace MOBase