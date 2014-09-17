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
#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdarg.h>
#include <syslog.h>
#include <strings.h>

#include "mesg.h"

#define LOG_ALWAYS -256

int mesg_level = LOG_INFO;
extern const char *progname;

/* set the message level, used by readconf.c */
/* TODO: fuzzier matching on strings */
/* TODO: conform to a new unified readconf validation function
 * symantics */
int
set_mesg_level(const char *level)
{
    int ret = 0;

    if (strcasecmp(level, "EMERG") == 0)
        mesg_level = LOG_ERR;

    else if (strcasecmp(level, "ALERT") == 0)
        mesg_level = LOG_ALERT;

    else if (strcasecmp(level, "CRIT") == 0)
        mesg_level = LOG_CRIT;

    else if (strcasecmp(level, "ERR") == 0)
        mesg_level = LOG_ERR;

    else if (strcasecmp(level, "WARNING") == 0)
        mesg_level = LOG_WARNING;

    else if (strcasecmp(level, "NOTICE") == 0)
        mesg_level = LOG_NOTICE;

    else if (strcasecmp(level, "INFO") == 0)
        mesg_level = LOG_INFO;

    else if (strcasecmp(level, "DEBUG") == 0)
        mesg_level = LOG_DEBUG;

    else
        ret = -1;

    return ret;
}

/* write the mesg message */
void
do_mesg(int level, const char *fmt, va_list args)
{
    if (level > mesg_level)
        return;

    char *buf;
    if (vasprintf(&buf, fmt, args) < 0)
        buf = "logging error, failed to allocate memory for buf";

    fprintf(stderr, "%s\n", buf);

    openlog(progname, LOG_PID, LOG_DAEMON);
    syslog(level, "%s", buf);
    closelog();

    free(buf);
}

/* fatal errors are logged and program is terminated */
/* XXX: OS X is really fucking annoying with LOG_EMERG level messages */
void
fatal(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
/*    do_mesg(LOG_EMERG, fmt, args); */
    do_mesg(LOG_ERR, fmt, args);
    va_end(args);
    _exit(255);
}

/* still probably going to terminate, but we dont have to leave now */
void
warn(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    do_mesg(LOG_ERR, fmt, args);
    va_end(args);
}

/* info mesgging */
void
info(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    do_mesg(LOG_INFO, fmt, args);
    va_end(args);
}

/* debugging should not be enabled under normal operation */
void
debug(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    do_mesg(LOG_DEBUG, fmt, args);
    va_end(args);
}

/* for when you always want to print a message */
void
mesg(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    do_mesg(LOG_ALWAYS, fmt, args);
    va_end(args);
}
