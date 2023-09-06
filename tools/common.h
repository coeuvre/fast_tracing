#pragma once

#include "src/buf.h"

// Split arg into a (key, value) pair.
void split_arg(char *arg, Buf *out_key, Buf *out_value);
