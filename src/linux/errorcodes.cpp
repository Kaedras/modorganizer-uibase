#include "../errorcodes.h"

#include <cstring>

QDLLEXPORT const char* errorCodeName(int code)
{
  return strerror(code);
}
