#ifndef STD_NET_READER_H
#define STD_NET_READER_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <setjmp.h>

/* Each handler owns its reader and abort point. A short packet must not turn
   into an unchecked pointer read, including in direct/savegame dispatch. */
static inline void stdNetRead(intptr_t *cursor,intptr_t end,void *out,size_t size,size_t alignment,jmp_buf abortRead) {
    uintptr_t at=(uintptr_t)*cursor,limit=(uintptr_t)end;
    size_t padding=(alignment-at%alignment)%alignment;
    if(at>limit||padding>limit-at||size>limit-at-padding)longjmp(abortRead,1);
    at+=padding;memcpy(out,(const void*)at,size);*cursor=(intptr_t)(at+size);
}
#endif
