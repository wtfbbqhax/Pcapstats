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
#include <stdint.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>

#include "getline.h"
#include "defragment.h"
#include "validate.h"
#include "readconf.h"
#include "mesg.h"

typedef enum {
    oBadOption,
    oLogLevel,
    oFlowAgeLimit, oFlowMaxMem,
    oFragAgeLimit, oFragMaxMem, oFragModel,
    oHostAgeLimit, oHostMaxMem,
    oUnsupported, oDeprecated
} Token;

typedef struct _Keyword {
    const char *name;
    Token token;
    void *option;
    int (*set)(const char *, const char *, unsigned, void *);
} Keyword;

static Keyword keywords[] = {
    { "LogLevel",       oLogLevel },
    { "FlowAgeLimit",   oFlowAgeLimit },
    { "FlowMaxMem",     oFlowMaxMem },
    { "FragAgeLimit",   oFragAgeLimit },
    { "FragMaxMem",     oFragMaxMem },
    { "FragModel",      oFragModel },
    { "HostMaxMem",     oHostMaxMem },
    { "HostAgeLimit",   oHostAgeLimit },
    { NULL,             oBadOption }
};

static Token
get_token(const char *keyword)
{
    for(int i = 0; keywords[i].name; i++)
        if (strcasecmp(keyword, keywords[i].name) == 0)
            return keywords[i].token;

    return oBadOption;
}

/* Comments are literally seen as white spaces to the parser.
 */
static void
process_comments(char s[])
{
    int i;
    for(i = 0; s[i] != '\0'; i++)
        if(s[i] == '#')
            break;
    for(;s[i] != '\0'; i++)
        s[i] = ' ';
}

static int
process_config_line(Options *opts,
    char *line, const char *filename, unsigned linenum)
{
    process_comments(line);

    while (*line && strchr(" \t\n", *line))
        ++line;

    /* empty lines go away */
    if (*line == '\0')
        return 0;

    /* Keyword starts here */
    const char *keyword = line;

    while (*line && !strchr(" \t\n", *line))
        ++line;

    if (*line)
        *line++ = '\0';

    /* Value starts here */
    const char *value = line;

    while (*line && !strchr(" \t\n", *line))
        ++line;

    if (*line)
        *line++ = '\0';

    while(*line && isblank(*line))
        ++line;

    int ret = 0;
    switch(get_token(keyword)) {
        /* Defaults */
        case oBadOption:
        warn("Bad Option \"%s\" at %s:%d",
            keyword, filename, linenum);
        ret = -1;
        break;

        case oUnsupported:
        warn("Unsupported Option \"%s\" at %s:%d",
            keyword, filename, linenum);
        ret = -1;
        break;

        case oDeprecated:
        warn("Deprecated Option \"%s\" at %s:%d",
            keyword, filename, linenum);
        ret = -1;
        break;

        /* Configurations */
        case oLogLevel:
        ret = set_mesg_level(value);
        if (ret == -1)
            warn("Bad log level value at %s:%d", filename, linenum);
        break;

        case oFragAgeLimit:
        opts->frag_age_limit =
            signed32_value(value, filename, linenum, &ret);
        break;

        case oFragMaxMem:
        opts->frag_max_mem =
            signed32_value(value, filename, linenum, &ret);
        if (opts->frag_max_mem < 1024) {
            warn("Minimum FragMaxMem value is 1024");
            ret = -1;
        }
        break;

        case oFragModel:
        set_defrag_method(value, filename, linenum, &ret);
        break;

        case oFlowMaxMem:
        opts->flow_max_mem =
            signed32_value(value, filename, linenum, &ret);
        if (opts->flow_max_mem < 1024) {
            warn("Minimum FlowMaxMem value is 1024");
            ret = -1;
        }
        break;

        case oFlowAgeLimit:
        opts->flow_age_limit =
            signed32_value(value, filename, linenum, &ret);
        break;

        case oHostMaxMem:
        opts->host_max_mem =
            signed32_value(value, filename, linenum, &ret);
        if (opts->flow_max_mem < 1024) {
            warn("Minimum HostMaxMem value is 1024");
            ret = -1;
        }
        break;

        case oHostAgeLimit:
        opts->host_age_limit =
            signed32_value(value, filename, linenum, &ret);
        break;
    }

    if (keyword && (!value || *value == '\0')) {
        warn("Missing Value for \"%s\" at %s:%d",
            keyword, filename, linenum);
        ret = -1;
    }

    if (isgraph(*line)) {
        warn("Junk at end of line %s:%d", filename, linenum);
        ret = -1;
    }

    return ret;
}

/* Read Configuration File
 *
 * Return
 * 1 on no configuration to load
 * 0 on success
 * -1 on warn
 */
int
read_config_file(const char *filename, Options *options)
{
    FILE *input;

    if ((input = fopen(filename, "r")) == NULL)
        return 1;

    info("Reading configuration file %s", filename);

    char *line = NULL;
    size_t len;
    unsigned linenum = 0;
    int ret = 0;

    while(getline(&line, &len, input) != -1) {
        linenum++;

        /* If we couldn't process the line, fail but I still want to
         * know about any other warns. */
        if (ret == -1)
            process_config_line(options, line, filename, linenum);
        else
            ret = process_config_line(options, line, filename,
                linenum);
    }

    fclose(input);

    if (line)
        free(line);

    return ret;
}

/* Reread Configuration File
 *
 * Return
 * 1 on no configuration to load
 * 0 on success
 * -1 on warn
 */
int
reload_config_file(const char *filename, Options *oldopts)
{
    int err = 0;
    Options newopts;
    memset(&newopts, 0, sizeof newopts);

    err = read_config_file(filename, &newopts);

    /* Hash table sizes can't be resized... yet */
    if (newopts.frag_max_mem != oldopts->frag_max_mem) {
        warn("Changing FragMaxMem requires are restart");
        err = -1;
    }

    if (newopts.flow_max_mem != oldopts->flow_max_mem) {
        warn("Changing FlowMaxMem requires are restart");
        err = -1;
    }

    /* Age limits can't properly be adjusted yet either */
    if (newopts.frag_age_limit != oldopts->frag_age_limit) {
        warn("Changing FragAgeLimit requires are restart");
        err = -1;
    }

    if (newopts.flow_age_limit != oldopts->flow_age_limit) {
        warn("Changing FlowAgeLimit requires are restart");
        err = -1;
    }
    if (err == 0)
        *oldopts = newopts;

    return err;

}
