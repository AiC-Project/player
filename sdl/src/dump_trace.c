/* vim:set ft=c ts=2 sw=2 sts=2 et cindent: */

#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buffer_sizes.h"

void dump_trace()
{
    void* buffer[BIG_BUF_SIZE];
    memset(buffer, 0, sizeof(buffer));
    const int calls = backtrace(buffer, sizeof(buffer) / sizeof(void*));
    printf("~ ~ ~ ~\n");
    printf("Oooh I'm gonna die. Good luck!\n\n");
    backtrace_symbols_fd(buffer, calls, 1);
    exit(EXIT_FAILURE);
}
