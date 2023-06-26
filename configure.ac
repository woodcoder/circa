AC_INIT([circa],
        m4_esyscmd([./version.sh]),
        [andy@bitshaper.co.uk])
AM_INIT_AUTOMAKE([foreign])
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])

LIBCURL_CHECK_CONFIG([], 7.15.2, [],
  AC_MSG_ERROR([Missing required libcurl >= 7.15.2]))
AC_CHECK_HEADER([curl/curl.h], [],
  AC_MSG_ERROR([Header curl/curl.h wasn't found]))
AC_CHECK_LIB([curl], [curl_easy_init], [],
  AC_MSG_ERROR([Missing required libcurl]))

AC_CHECK_HEADER([jansson.h], [],
  AC_MSG_ERROR([Header jansson.h wasn't found]))
AC_CHECK_LIB([jansson], [json_array_set_new], [],
  AC_MSG_ERROR([Missing required libjansson]))

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
