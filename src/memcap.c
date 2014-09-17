/* Copyright (c) 2010-2012, Victor J. Roemer. All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* memcap.c
 *
 * Victor Roemer (vroemer@evilsec.org)
 *
 * An imitation of Snort's memory capping capabilities.
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memcap.h"

struct Memcap
{
    size_t memcap;
    size_t allocated;
};

Memcap *
memcap_create(size_t bytes)
{
    Memcap *memcap = malloc(sizeof *memcap);

    if (memcap == NULL)
        return NULL;

    memcap->memcap = bytes;
    memcap->allocated = 0;

    return memcap;
}

int
memcap_destroy(Memcap *memcap)
{
    if (memcap->allocated)
        return -1;

    free(memcap);
    return 0;
}

void *
memcap_alloc(Memcap *memcap, size_t size)
{
    size += sizeof(size_t);

    if ((memcap->allocated + size) > memcap->memcap)
        return NULL;

    size_t *block = malloc(size);
    if (block == NULL)
        return NULL;

    *block++ = size;

    memcap->allocated += size;

    return block;
}

void *
memcap_calloc(Memcap *memcap, size_t nmemb, size_t size)
{
    size *= nmemb;
    size += sizeof(size_t);

    if ((memcap->allocated + size) > memcap->memcap)
        return NULL;

    size_t *block = malloc(size);
    if (block == NULL)
        return NULL;

    *block++ = size;

    memset((void *)block, 0, size-sizeof(size_t));

    memcap->allocated += size;

    return block;
}

void
memcap_free(Memcap *memcap, void *block)
{
    size_t *size = (size_t *)block;
    size--;

    memcap->allocated -= *size;

    free(size);
}

//#ifdef DEBUG
void
memcap_dump(Memcap *memcap)
{
    printf("Memcap Cap %lu\n", memcap->memcap);
    printf("Memcap Allocated %lu\n", memcap->allocated);
}
//#endif

//#if 0
int main( )
{
    void *memcap = memcap_create(16*1024);
    if (memcap == NULL)
        return 2;

    memcap_dump(memcap);

    void *data = memcap_alloc(memcap, 100);
    if (data == NULL)
        return 2;

    memcap_dump(memcap);

    memcap_free(memcap, data);

//    data = memcap_alloc(memcap, 10 * 1024);
    data = memcap_calloc(memcap, 10, 1024);

    memcap_dump(memcap);

    memcap_free(memcap, data);

    memcap_dump(memcap);

    return 0;
}
//#endif
