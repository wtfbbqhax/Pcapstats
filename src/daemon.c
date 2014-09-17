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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <syslog.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <errno.h>

#include "mesg.h"

/* Run as a daemon */
int
daemonize()
{
    if (geteuid() != 0)
    {
        errno = EPERM;
        return -1;
    }

    /* Background process */
    switch(fork( ))
    {
        case -1: return -1;
        case  0: break;
        default: exit( 0 );
    }

    /* Become session leader */
    if(setsid() == -1)
        return -1;

    /* Ow, wait.. I don't want to be session leader */
    switch(fork( ))
    {
        case -1: return -1;
        case  0: break;
        default: exit( 0 );
    }

    umask( 0 );

    if (chdir("/") < 0)
        return -1;

    /* Close all open file descriptors */
    struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) < 0)
        return -1;

    if (rlim.rlim_max == RLIM_INFINITY)
        rlim.rlim_max = 1024;

    for (rlim_t i = 0; i < rlim.rlim_max; i++)
        close(i);

    /* Reopen stdin, stdout, and stderr to /dev/null */
    int fd0 = open("/dev/null", O_RDWR);
    int fd1 = dup(0);
    int fd2 = dup(0);

    if(fd0 != 0 || fd1 != 1 || fd2 != 2)
        return -1;

    return 0;
}
