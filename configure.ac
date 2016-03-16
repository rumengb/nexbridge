#                                              -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(nexbridge,0.4-b1, rumen@skyarchive.org)
AM_INIT_AUTOMAKE(nexbridge,0.4-b1)
AC_CONFIG_SRCDIR([src/nexbridge.c])
AC_CONFIG_HEADER([src/config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB(nsl,gethostbyname , LIBS="$LIBS -lnsl",)
AC_CHECK_LIB(socket, accept, LIBS="$LIBS -lsocket",)
AC_CHECK_LIB(resolv,inet_aton, LIBS="$LIBS -lresolv",)
AC_CHECK_LIB(pthread,pthread_create, LIBS="$LIBS -lpthread",)
#AC_CHECK_LIB(avahi-client,avahi_client_new, LIBS="$LIBS -lavahi-client -lavahi-common",)
AC_CHECK_LIB(avahi-client,avahi_client_new,,
        [
                echo "==================================================";
                echo "WARNING: avahi-client not found, mDNS is disabled!";
                echo "=================================================="
        ]
)

AC_CHECK_LIB(avahi-common,avahi_simple_poll_new,,
        [
                echo "==================================================";
                echo "WARNING: avahi-common not found, mDNS is disabled!";
                echo "=================================================="
        ]
)



# Checks for header files.
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STRTOD

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
test ${os_type} = "Darwin" && default_device="/dev/cu.usbserial"

AC_DEFINE_UNQUOTED(TTY_PORT, "${default_device}", [Default Device serial port])
AC_DEFINE_UNQUOTED(PORT, 9999, [Default TCP port])
AC_DEFINE_UNQUOTED(DATA_FORMAT, "8N1", [Default serial data format])
AC_DEFINE_UNQUOTED(BAUDRATE, "9600", [Default serial baud rate])
AC_DEFINE_UNQUOTED(MAXCON, 1, [Default Max clients])
AC_DEFINE_UNQUOTED(SESS_TIMEOUT, 0, [Session timeout])
AC_DEFINE_UNQUOTED(RECONNECT_TIME, 3, [Default interval between reconnects for ttynet in seconds])
AC_DEFINE_UNQUOTED(SVC_TYPE, "_nexbridge", [Bonjour service name])
AC_DEFINE_UNQUOTED(SVC_PROTO, "_tcp", [Bonjour service type])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
