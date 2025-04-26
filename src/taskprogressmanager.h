#pragma once

#ifdef _WIN32
#include "win32/taskprogressmanager.h"
#else
#include "linux/taskprogressmanager.h"
#endif
