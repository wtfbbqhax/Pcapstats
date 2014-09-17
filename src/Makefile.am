AUTOMAKE_OPTIONS = foreign no-dependencies
bin_PROGRAMS=pcapstats

#
# Pcapstats 
#
pcapstats_SOURCES=  \
    cdefs.h \
    pcapstats.c \
	daemon.c daemon.h \
    mesg.c mesg.h \
    validate.c validate.h \
    readconf.c readconf.h \
    defragment.c \
    defragment.h \
	flow.c flow.h \
    host.c host.h \
	stream-tcp.c stream-tcp.h \
	tcp-state.c tcp-state.h

pcapstats_LDADD = libutil.la

#
# Supporting libraries
#
noinst_LTLIBRARIES = libutil.la

libutil_la_SOURCES  = hashtable.c hashtable.h 
libutil_la_SOURCES += hashdigest.c hashdigest.h
libutil_la_SOURCES += timequeue.c timequeue.h

if DEBUG
libutil_la_SOURCES += print-data.c print-data.h
endif

if USE_GETLINE
libutil_la_SOURCES += getline.c getline.h
endif

AM_CPPFLAGS = -Wall -Wextra -Wformat -Wformat-security -pedantic
pcapstats_CPPFLAGS = -DSYSCONFDIR='"$(sysconfdir)"'
