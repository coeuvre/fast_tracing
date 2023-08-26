#ifndef FAST_TRACING_TOOLS_COMMON_H_
#define FAST_TRACING_TOOLS_COMMON_H_

#include "src/common.h"

// Split arg into a (key, value) pair.
void SplitArg(char *arg, Buf *out_key, Buf *out_value);

#endif // FAST_TRACING_TOOLS_COMMON_H_