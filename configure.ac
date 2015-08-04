AC_PREREQ([2.65])
AC_INIT([chiasm], [0.1], [kingston.zak@gmail.com])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([include/chiasm.h])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_HEADER_STDC

LT_INIT([dlopen])

AC_PROG_CC_C99
AC_ENABLE_SHARED
AC_DISABLE_STATIC

AC_C_INLINE
AC_TYPE_SIZE_T

AC_FUNC_ALLOCA
AC_FUNC_ERROR_AT_LINE

AC_SEARCH_LIBS([dlopen], [dl dld], [],
               [AC_MSG_ERROR([Unable to find dlopen()])])
AC_SEARCH_LIBS([pthread_create], [pthread], [],
               [AC_MSG_ERROR([Unable to find pthread_create()])])
AC_SEARCH_LIBS([av_register_all], [avformat], [],
               [AC_MSG_ERROR([Unable to find av_register_all()])])
AC_SEARCH_LIBS([avcodec_find_decoder], [avcodec], [],
               [AC_MSG_ERROR([Unable to find sws_scale()])])
AC_SEARCH_LIBS([sws_scale], [swscale], [],
               [AC_MSG_ERROR([Unable to find sws_scale()])])
AC_SEARCH_LIBS([tag36h11_create], [apriltag], [],
               [AC_MSG_ERROR([Unable to find tag36h11_create()])])
AC_SEARCH_LIBS([aa_tf_tfmat2duqu], [amino], [],
               [AC_MSG_ERROR([Unable to find aa_tf_tfmat2duqu()])])

PKG_CHECK_MODULES([OPENCV], [opencv])
PKG_CHECK_MODULES([GTK], [gtk+-3.0])

AX_CFLAGS_WARN_ALL

AC_DEFUN([APPEND_CFLAG], [AX_CHECK_COMPILE_FLAG([$1], [CFLAGS="$1 $CFLAGS"])])
APPEND_CFLAG([-Wextra])
APPEND_CFLAG([-Wpointer-arith])
APPEND_CFLAG([-Wfloat-equal])
APPEND_CFLAG([-Wshadow])
APPEND_CFLAG([-Wwrite-strings])
APPEND_CFLAG([-Wc++-compat])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
