#include <windows.h>
#include <shlwapi.h>
#include <intrin.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#undef EXIT_FAILURE
#define EXIT_FAILURE doorstop_exit_failure
static const int doorstop_exit_failure = 1;
#define memset doorstop_memset
#define memcpy doorstop_memcpy
#define wmemcpy doorstop_wmemcpy
#define wmemset doorstop_wmemset
#define wcslen doorstop_wcslen

#include "main.c"
