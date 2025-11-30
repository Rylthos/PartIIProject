#pragma once

#include "tracy/Tracy.hpp"

#ifdef DEBUG
#define TRACE_FRAME_MARK FrameMark
#else
#define TRACE_FRAME_MARK
#endif
