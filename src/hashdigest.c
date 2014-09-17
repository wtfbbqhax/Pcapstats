#include <sys/types.h>
#include "hashdigest.h"
#include <stdlib.h>
#include <time.h>

static int seed;

void digest_init(unsigned buckets)
{
    srand(time(0));
    seed = (rand() * rand() % buckets);
}

unsigned long fnv1a_digest(const void *buf, size_t len, unsigned long hval)
{
    unsigned char *start = (unsigned char *)buf;
    unsigned char *end = start + len;

    while (start < end) {
        hval ^= (unsigned char)*start++;
        hval *= 0x01000193;
        hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
    }

    return hval + seed;
}
