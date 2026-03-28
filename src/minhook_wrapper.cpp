/**
 * MinHook Wrapper for C++ compilation
 * This file includes MinHook C source files with proper extern "C" linkage
 */

extern "C" {
#include "buffer.c"
#include "hook.c"
#include "trampoline.c"
#include "hde64.c"
}
