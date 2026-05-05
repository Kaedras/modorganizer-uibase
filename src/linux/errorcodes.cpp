#include "errorcodes.h"
#include <cstring>

namespace MOBase
{

const char* errorCodeName(int code)
{
  return strerror(code);
}

}  // namespace MOBase
