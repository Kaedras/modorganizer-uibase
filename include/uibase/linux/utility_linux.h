#ifndef UTILITY_LINUX_H
#define UTILITY_LINUX_H

#include "compatibility.h"
#include "fdcloser.h"

#include <memory>

namespace MOBase::shell::details
{

using HandlePtr = FdCloser;

}

#endif  // UTILITY_LINUX_H
