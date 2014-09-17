/* Copyright (c) 2012, Victor J Roemer. All Rights Reserved.
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

//
//  hashtable.h
//  pcapstats
//
//  Lightweight hashtable implementation in C.
//
//  Created by Victor J. Roemer on 3/4/12.
//  Copyright (c) 2012 Victor J. Roemer. All rights reserved.
//

#ifndef hashtable_h
#define hashtable_h

#include <stdlib.h>
#include <stdbool.h>


typedef struct _Hash Hash;

#define hash_size(table) ((table)->size)

typedef void *(*alloc_t)(size_t size);
typedef void (*free_t)(void *ptr);

Hash *hash_create(size_t buckets);

void hash_destroy(Hash *this);

int hash_insert(Hash *table, void *data, void *key, size_t keysize);

void *hash_remove(Hash *this, const void *key, size_t keysize);

void *hash_first(Hash *this, unsigned *it, const void **key);

void *hash_next(Hash *this, unsigned *it, const void **key);

void *hash_get(Hash *this, void *key, size_t keysize);

void hash_dump(Hash *table);

#endif /* hashtable_h */
