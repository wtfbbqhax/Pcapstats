# Victor Roemer (sploit@evilsec.org)
#
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([wiretap], [0.0.1], [bugs@evilsec.org])
AC_CONFIG_SRCDIR([src/pcapstats.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

#
# Checks for programs.
#
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
LT_INIT
AC_PROG_LIBTOOL

#
# Check for LibPCAP
#
AC_ARG_WITH(libpcap, [AS_HELP_STRING(
    [--with-libpcap=PATH],
    [Specify prefix directory for the installed libpcap.])])

# Error if we didn't find libpcap
AC_CHECK_LIB([pcap], [pcap_open_live], , 
    AC_MSG_ERROR(
    [

    Error! libpcap library/headers not found.
    Get it from http://tcpdump.org or use the --with-libpcap option if its
    installed in an unusual place.
    ]))

#
# Check for LibPacket
#
AC_ARG_WITH(libpacket, [AS_HELP_STRING(
    [--with-libpacket=PATH],
    [Specify prefix directory for the installed libpacket.])])

# Error if we didn't find libPacket
AC_CHECK_LIB([packet], [packet_decode], , 
    AC_MSG_ERROR(
    [

    Error! libpacket library/headers not found.
    Get it from git.evilsec.org or use the --with-libpacket option if its
    installed in an unusual place.
    ]))

#
# Setup for host specific needs 
#
case "$host" in
    *-darwin*)
    AC_DEFINE([MACOS], [1], [Define if MacOS])
    ;;

    # OpenBSD is the only truelly special box as of now
    *-openbsd*)
    AC_DEFINE([OPENBSD], [1], [Define if OpenBSD])
    ;;

    *-linux*)
    AC_DEFINE([LINUX], [1], [Define if Linux])
    ;;
esac

#
# Determine whether or not to compile with pthreads support
#
# NOTE: this should not be used for now since the hashtable is not
#  thread safe. 
#
AC_ARG_ENABLE(pthreads,
[  --enable-pthreads       Enable pthread support],
       enable_pthread="$enableval", enable_pthread="no")

if test "x$enable_pthread" = "xyes"; then
    AC_DEFINE([ENABLE_PTHREADS],[1],[Define if pthreads use is desired])
    LIBS="$LIBS -lpthread"
fi

#
# Enable Development features
#
AC_ARG_ENABLE(debug,
[  --enable-debug          Enable debug mode],
    enable_debug="$enableval", enable_debug="no")

AM_CONDITIONAL(DEBUG, test "x$enable_debug" == "xyes")

if test "x$enable_debug" = "xyes"; then
    CFLAGS="${CFLAGS} -g -ggdb"
    CPPFLAGS="${CPPFLAGS} -DDEBUG"
#    AC_DEFINE([ENABLE_DEBUG],[1],[Enable development features])
fi

#
# Enable builtin functions
#
AC_ARG_ENABLE([builtin],
[  --enable-builtin        Enable builtin functions],
    enable_builtin="$enableval", enable_builtin="no")

#AM_CONDITIONAL(ENABLE_BUILTIN, test "x$enable_builtin" == "xyes")

#
# Checks for header files.
#
AC_HEADER_STDC
AC_CHECK_HEADERS([sys/types.h],, AC_ERROR([Missing header sys/types.h]))
AC_CHECK_HEADERS([sys/socket.h],, AC_ERROR([Missing header sys/socket.h]))
AC_CHECK_HEADERS([netinet/in.h],, AC_ERROR([Missing header netinet/in.h]))
AC_CHECK_HEADERS([arpa/inet.h],, AC_ERROR([Missing header arpa/inet.h]))
AC_CHECK_HEADERS([fcntl.h],, AC_ERROR([Missing header fcntl.h]))
AC_CHECK_HEADERS([limits.h],, AC_ERROR([Missing header limits.h]))
AC_CHECK_HEADERS([stdint.h],, AC_ERROR([Missing header stdint.h]))
AC_CHECK_HEADERS([inttypes.h],, AC_ERROR([Missing header inttypes.h]))
AC_CHECK_HEADERS([stdbool.h],, AC_ERROR([Missing header stdbool.h]))
AC_CHECK_HEADERS([stdlib.h],, AC_ERROR([Missing header stdlib.h]))
AC_CHECK_HEADERS([string.h],, AC_ERROR([Missing header string.h]))
AC_CHECK_HEADERS([strings.h],, AC_ERROR([Missing header strings.h]))
AC_CHECK_HEADERS([sys/time.h],, AC_ERROR([Missing header sys/time.h]))
AC_CHECK_HEADERS([syslog.h],, AC_ERROR([Missing header syslog.h]))
AC_CHECK_HEADERS([unistd.h],, AC_ERROR([Missing header unistd.h]))

#
# Checks for typedefs, structures, and compiler characteristics.
#
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

#
# Check for functions  which I DONT have builtins for
#
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([gettimeofday],, AC_ERROR([Missing function gettimeofday]))
AC_CHECK_FUNCS([memset],, AC_ERROR([Missing function memset]))
AC_CHECK_FUNCS([strcasecmp],, AC_ERROR([Missing function strcasecmp]))
AC_CHECK_FUNCS([strchr],, AC_ERROR([Missing function strchr]))
AC_CHECK_FUNCS([strerror],, AC_ERROR([Missing function strerror]))
AC_CHECK_FUNCS([strtol],, AC_ERROR([Missing function strtol]))

# 
# Check for functions which I DO have builtins for 
#
AC_CHECK_FUNCS([getline])
AM_CONDITIONAL(USE_GETLINE, test "x$ac_cv_func_getline" != "xyes")
AM_CONDITIONAL(USE_GETLINE, test "x$enable_builtin" == "xyes")

#
# Set the package configuration filename
#
AC_DEFINE_UNQUOTED([PACKAGE_CONFIG_FILE], 
    "${PACKAGE}.conf", [Configuration file for the package])

# 
# Write the configuration files
#
AC_CONFIG_FILES([Makefile
                 etc/Makefile
                 doc/Makefile
                 doc/pcapstats.8
                 src/Makefile])

# 
# Output the data
#
AC_OUTPUT
