#pragma once

#ifdef __unix__
#include "linux/taskprogressmanager.h"
#else
#include "win32/taskprogressmanager.h"
#endif
