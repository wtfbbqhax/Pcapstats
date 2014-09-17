//
//  digest.h
//  pcapstats
//
//  Created by Victor J. Roemer on 3/6/12.
//  Copyright (c) 2012 Victor J. Roemer. All rights reserved.
//
//
#include <sys/types.h>

void digest_init(unsigned buckets);
unsigned long fnv1a_digest(const void *buf, size_t len, unsigned long hval);
