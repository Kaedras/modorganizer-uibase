#include "errorcodes.h"

namespace MOBase
{

const char* errorCodeName(int code)
{
  return strerror(code);
}

}  // namespace MOBase
