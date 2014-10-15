#                                              -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(nexbridge,0.01-b1, rumen@skyarchive.org)
AM_INIT_AUTOMAKE(nexbridge,0.01-b1)
AC_CONFIG_SRCDIR([src/nexbridge.c])
AC_CONFIG_HEADER([src/config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
#AC_CHECK_LIB(m, floor, LIBS="$LIBS -lm",)
AC_CHECK_LIB(nsl,gethostbyname , LIBS="$LIBS -lnsl",)
AC_CHECK_LIB(socket, accept, LIBS="$LIBS -lsocket",)
AC_CHECK_LIB(resolv,inet_aton, LIBS="$LIBS -lresolv",)
AC_CHECK_LIB(pthread,pthread_create, LIBS="$LIBS -lpthread",)
AC_CHECK_LIB(avahi-client,avahi_client_new, LIBS="$LIBS -lavahi-client -lavahi-common",)

# Checks for header files.
AC_HEADER_STDC
#AC_CHECK_HEADERS([arpa/inet.h fcntl.h stdlib.h string.h strings.h sys/socket.h sys/time.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STRTOD
#AC_CHECK_FUNCS([gettimeofday inet_ntoa memset select socket])

test "x${prefix}" = "xNONE" && prefix=${ac_default_prefix}
test "x${exec_prefix}" = "xNONE" && exec_prefix=${prefix}

AC_DEFINE_UNQUOTED(PREFIX_DIR, "${prefix}", [Prefix])
AC_DEFINE_UNQUOTED(BIN_DIR, "`eval echo $bindir`", [Executable Directory])

os_type=`uname`;
default_device="/dev/unknown"
AC_DEFINE_UNQUOTED(GUESSED_OS_TYPE, "${os_type}", [Guesed Target Operating System])
AC_DEFINE_UNQUOTED(GUESSED_HARDWARE, "`uname -i`", [Guesed Target Hardware])
test ${os_type} = "SunOS" && default_device="/dev/ttya"
test ${os_type} = "Linux" && default_device="/dev/ttyUSB0"
test ${os_type} = "IRIX" && default_device="/dev/ttyf1"

AC_DEFINE_UNQUOTED(DEFAULT_DEVICE, "${default_device}", [Default Device serial port])
AC_DEFINE_UNQUOTED(DEFAULT_PORT, 9999, [Default TCP port])
AC_DEFINE_UNQUOTED(DEFAULT_MAXCON, 1, [Default Max clients])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
