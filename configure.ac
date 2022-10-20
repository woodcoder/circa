AC_INIT([circa],
        m4_translit(m4_esyscmd([git describe --abbrev=4 --match="v*" HEAD]), '-vg', '.'),
        [andy@bitshaper.co.uk])
AM_INIT_AUTOMAKE([foreign])
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])
LIBCURL_CHECK_CONFIG([], 7.15.2, [],
  AC_MSG_ERROR([Missing required libcurl >= 7.15.2]))
AC_CHECK_LIB([jansson], [json_array_set_new], [],
  AC_MSG_ERROR([Missing required libjansson]))
AC_CHECK_LIB([curl], [curl_easy_init], [],
  AC_MSG_ERROR([Missing required libcurl]))
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
