AC_INIT([circa], [0.1], [andy@bitshaper.co.uk])
AM_INIT_AUTOMAKE([foreign])
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])
AC_CHECK_LIB([jansson], [json_array_set_new], [],
  AC_MSG_ERROR([Missing required libjansson]))
LIBCURL_CHECK_CONFIG([], 7.15.2, [],
  AC_MSG_ERROR([Missing required libcurl >= 7.15.2]))
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
