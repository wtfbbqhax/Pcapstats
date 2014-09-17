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
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <stdbool.h>
#include <math.h>

#include <strings.h>

#include <errno.h>

#include "mesg.h"

/* Validate a boolean value */
bool
boolean_value(const char *value, const char *filename,
    unsigned linenum, int *error)
{
    if (strcasecmp(value, "true") == 0)
        return true;
    else if (strcasecmp(value, "false") == 0)
        return false;
    else {
        warn("Bad boolean value at %s:%d", filename, linenum);
        *error = -1;
    }
    return false;
}

/* Validate a signed long long value */
int64_t
signed64_value(const char *value, const char *filename, 
    unsigned linenum, int *error)
{
    char *endptr;
    uint64_t ret;

    errno = 0;
    ret = strtoll(value, &endptr, 0);

    if (errno == ERANGE) {
        warn("Value out of range %s:%d", filename, linenum);
        *error = -1;
    }

    if (endptr == value) {
        warn("Bad integer value at %s:%d", filename, linenum);
        *error = -1;
    }

    return ret;
}

/* Validate a signed long value */
int32_t
signed32_value(const char *value, const char *filename, 
    unsigned linenum, int *error)
{
    char *endptr;
    uint32_t ret;

    errno = 0;
    ret = strtol(value, &endptr, 0);

    if (errno == ERANGE) {
        warn("Value out of range %s:%d", filename, linenum);
        *error = -1;
    }

    if (endptr == value) {
        warn("Bad integer value at %s:%d", filename, linenum);
        *error = -1;
    }

    return ret;
}

/* Validate a signed short value */
int16_t
signed16_value(const char *value, const char *filename, 
    unsigned linenum, int *error)
{
    uint32_t ret = signed32_value(value, filename, linenum, error);

    if((*error == 0) &&
        (ret > (uint32_t)SHRT_MAX || ret < (uint32_t)SHRT_MIN)) {
        warn("Value out of range %s:%d", filename, linenum);
        *error = -1;
    }

    return ret;
}

/* Validate a floating point value */
float
float_value(const char *value, const char *filename,
    unsigned linenum, int *error)
{
    char *endptr;
    float ret;

    errno = 0;
    ret = strtof(value, &endptr);

    if (errno == ERANGE) {
        warn("Value out of range %s:%d", filename, linenum);
        *error = -1;
    }

    if (endptr == value) {
        warn("Bad floating-point value at %s:%d", filename, linenum);
        *error = -1;
    }

    if (isnan(ret) || isinf(ret)) {
        warn("Value out of range %s:%d", filename, linenum);
        *error = -1;
    }

    return ret;
}
