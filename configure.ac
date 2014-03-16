#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([toxic], [0.3.0], [https://tox.im/])
AC_CONFIG_AUX_DIR(configure_aux)
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.10 -Wall])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_MACRO_DIR([m4])

if test "x${prefix}" = "xNONE"; then
    prefix="${ac_default_prefix}"
fi

DEPSEARCH=
LIBTOXCORE_SEARCH_HEADERS=
LIBTOXCORE_SEARCH_LIBS=
LIBSODIUM_SEARCH_HEADERS=
LIBSODIUM_SEARCH_LIBS=

LIBTOXCORE_FOUND="no"
NCURSES_FOUND="no"
NCURSES_WIDECHAR_SUPPORT="no"

AC_ARG_WITH(dependency-search,
    AC_HELP_STRING([--with-dependency-search=DIR],
                   [search for dependencies in DIR, i.e. look for libraries in
                    DIR/lib and for headers in DIR/include]),
    [
        DEPSEARCH="$withval"
    ]
)

if test -n "$DEPSEARCH"; then
    CFLAGS="$CFLAGS -I$DEPSEARCH/include"
    CPPFLAGS="$CPPFLAGS -I$DEPSEARCH/include"
    LDFLAGS="$LDFLAGS -L$DEPSEARCH/lib"
    export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$DEPSEARCH/lib/pkgconfig
fi

AC_ARG_WITH(libtoxcore-headers,
        AC_HELP_STRING([--with-libtoxcore-headers=DIR],
                       [search for libtoxcore header files in DIR/tox]),
        [
            LIBTOXCORE_SEARCH_HEADERS="$withval"
            AC_MSG_NOTICE([Will search for libtoxcore header files in $withval])
        ]
)

AC_ARG_WITH(libtoxcore-libs,
        AC_HELP_STRING([--with-libtoxcore-libs=DIR],
                       [search for libtoxcore libraries in DIR]),
        [
            LIBTOXCORE_SEARCH_LIBS="$withval"
            AC_MSG_NOTICE([Will search for libtoxcore libraries in $withval])
        ]
)

AC_ARG_WITH(libsodium-headers,
        AC_HELP_STRING([--with-libsodium-headers=DIR],
                       [search for libsodium header files in DIR]),
        [
            LIBSODIUM_SEARCH_HEADERS="$withval"
            AC_MSG_NOTICE([Will search for libsodium header files in $withval])
        ]
)

AC_ARG_WITH(libsodium-libs,
        AC_HELP_STRING([--with-libsodium-libs=DIR],
                       [search for libsodium libraries in DIR]),
        [
            LIBSODIUM_SEARCH_LIBS="$withval"
            AC_MSG_NOTICE([Will search for libsodium libraries in $withval])
        ]
)

WIN32=no
AC_CANONICAL_HOST
case $host_os in
    *mingw*)
        WIN32="yes"
    ;;
    *freebsd*)
        LDFLAGS="$LDFLAGS -L/usr/local/lib"
        CFLAGS="$CFLAGS -I/usr/local/include"
        CPPFLAGS="$CPPFLAGS -I/usr/local/include"
    ;;
esac

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

AC_CHECK_HEADERS(
    [limits.h locale.h stdint.h stdlib.h string.h unistd.h wchar.h wctype.h],
    [],
    [ AC_MSG_ERROR([required header is missing on your system]) ])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS(
    [iswprint memmove memset mkdir setlocale strchr strdup],
    [],
    [ AC_MSG_ERROR([required library function is missing on your system])])

AX_PTHREAD( [], [ AC_MSG_ERROR([pthreads not found on your system]) ])

# pkg-config based tests
PKG_PROG_PKG_CONFIG

if test -n "$PKG_CONFIG"; then
    if test "x$WIN32" != "xyes"; then
        PKG_CHECK_MODULES([NCURSES], [ncursesw],
            [
                NCURSES_FOUND="yes"
                NCURSES_WIDECHAR_SUPPORT="yes"
            ],
            [
                NCURSES_WIDECHAR_SUPPORT="no"
                PKG_CHECK_MODULES([NCURSES], [ncurses],
                    [
                        NCURSES_FOUND="yes"
                    ],
                    [
                        AC_MSG_WARN([$NCURSES_PKG_ERRORS])
                    ])
            ])
    fi
else
    AC_MSG_WARN([pkg-config was not found on your sytem])
fi

if (test "x$NCURSES_FOUND" = "xno") && (test "x$WIN32" != "xyes"); then
    AC_PATH_PROG([CURSES_CONFIG], [ncursesw5-config], [no])
    if test "x$CURSES_CONFIG" != "xno"; then
        AC_MSG_CHECKING(ncurses cflags)
        NCURSES_CFLAGS=`${CURSES_CONFIG} --cflags`
        AC_MSG_RESULT($NCURSES_CFLAGS)

        AC_MSG_CHECKING(ncurses libraries)
        NCURSES_LIBS=`${CURSES_CONFIG} --libs`
        AC_MSG_RESULT($NCURSES_LIBS)

        AC_SUBST(NCURSES_CFLAGS)
        AC_SUBST(NCURSES_LIBS)
        NCURSES_FOUND="yes"
        NCURSES_WIDECHAR_SUPPORT="yes"
    fi
fi

if (test "x$NCURSES_FOUND" = "xno") && (test "x$WIN32" != "xyes"); then
    unset ac_cv_path_CURSES_CONFIG
    AC_PATH_PROG([CURSES_CONFIG], [ncursesw5.4-config], [no])
    if test "x$CURSES_CONFIG" != "xno"; then
        AC_MSG_CHECKING(ncurses cflags)
        NCURSES_CFLAGS=`${CURSES_CONFIG} --cflags`
        AC_MSG_RESULT($NCURSES_CFLAGS)

        AC_MSG_CHECKING(ncurses libraries)
        NCURSES_LIBS=`${CURSES_CONFIG} --libs`
        AC_MSG_RESULT($NCURSES_LIBS)

        AC_SUBST(NCURSES_CFLAGS)
        AC_SUBST(NCURSES_LIBS)
        NCURSES_FOUND="yes"
        NCURSES_WIDECHAR_SUPPORT="yes"
    fi
fi

if (test "x$NCURSES_FOUND" = "xno") && (test "x$WIN32" != "xyes"); then
    unset ac_cv_path_CURSES_CONFIG
    AC_PATH_PROG([CURSES_CONFIG], [ncurses5-config], [no])
    if test "x$CURSES_CONFIG" != "xno"; then
        AC_MSG_CHECKING(ncurses cflags)
        NCURSES_CFLAGS=`${CURSES_CONFIG} --cflags`
        AC_MSG_RESULT($NCURSES_CFLAGS)

        AC_MSG_CHECKING(ncurses libraries)
        NCURSES_LIBS=`${CURSES_CONFIG} --libs`
        AC_MSG_RESULT($NCURSES_LIBS)

        AC_SUBST(NCURSES_CFLAGS)
        AC_SUBST(NCURSES_LIBS)
        NCURSES_FOUND="yes"
    fi
fi

if (test "x$NCURSES_FOUND" = "xno") && (test "x$WIN32" != "xyes"); then
    unset ac_cv_path_CURSES_CONFIG
    AC_PATH_PROG([CURSES_CONFIG], [ncurses5.4-config], [no])
    if test "x$CURSES_CONFIG" != "xno"; then
        AC_MSG_CHECKING(ncurses cflags)
        NCURSES_CFLAGS=`${CURSES_CONFIG} --cflags`
        AC_MSG_RESULT($NCURSES_CFLAGS)

        AC_MSG_CHECKING(ncurses libraries)
        NCURSES_LIBS=`${CURSES_CONFIG} --libs`
        AC_MSG_RESULT($NCURSES_LIBS)

        AC_SUBST(NCURSES_CFLAGS)
        AC_SUBST(NCURSES_LIBS)
        NCURSES_FOUND="yes"
    fi
fi

if test "x$NCURSES_FOUND" = "xno"; then
    AC_CHECK_HEADER([curses.h],
        [],
        [
            AC_MSG_ERROR([headers for the ncurses library were not found on your system])
        ]
    )

    if test "x$WIN32" = "xyes"; then
        dnl Check if pdcurses provides wide char support
        NCURSES_WIDECHAR_SUPPORT="no"
        AC_CHECK_LIB([pdcurses], [clear],
            [],
            [
                AC_MSG_ERROR([required library pdcurses was not found on your system])
            ]
        )

        AC_CHECK_LIB(ws2_32, main,
            [
                WINSOCK2_LIBS="-lws2_32"
                AC_SUBST(WINSOCK2_LIBS)
            ],
            [
                AC_MSG_ERROR([required library winsock2 was not found on the system, please check your MinGW installation])
            ]
        )
        AC_DEFINE([_WIN32_WINNT], [0x501],
                  [enable getaddrinfo/freeaddrinfo on XP and higher])
    else
        AC_CHECK_LIB([ncursesw], [wget_wch],
            [
                NCURSES_WIDECHAR_SUPPORT="yes"
            ],
            [
                unset ac_cv_lib_ncursesw_wget_wch
                AC_CHECK_LIB([ncursesw], [wget_wch],
                    [
                        NCURSES_WIDECHAR_SUPPORT="yes"
                    ],
                    [
                        NCURSES_WIDECHAR_SUPPORT="no"
                        AC_CHECK_LIB([ncurses], [clear],
                            [],
                            [
                                unset ac_cv_lib_ncurses_clear
                                AC_CHECK_LIB([ncurses], [clear],
                                    [],
                                    [
                                        AC_MSG_ERROR([required library ncurses was not found on your system])
                                    ],
                                    [
                                        -ltinfo
                                    ]
                                )
                            ]
                        )
                    ],
                    [
                        -ltinfo
                    ]
                )
            ]
        )
    fi
fi

if test -n "$PKG_CONFIG"; then
    PKG_CHECK_MODULES(LIBTOXCORE, [libtoxcore],
        [
            LIBTOXCORE_FOUND="yes"
        ],
        [
            AC_MSG_WARN([required library libsodium was not found in requested location $LIBSODIUM_SEARCH_LIBS])
        ])
fi

if test "x$LIBTOXCORE_FOUND" = "xno"; then
    LIBSODIUM_LIBS=
    LIBSODIUM_LDFLAGS=
    LDFLAGS_SAVE="$LDFLAGS"
    if test -n "$LIBSODIUM_SEARCH_LIBS"; then
        LDFLAGS="$LDFLAGS -L$LIBSODIUM_SEARCH_LIBS"
        AC_CHECK_LIB(sodium, randombytes_random,
            [
                LIBSODIUM_LDFLAGS="-L$LIBSODIUM_SEARCH_LIBS"
                LIBSODIUM_LIBS="-lsodium"
            ],
            [
                AC_MSG_ERROR([required library libsodium was not found in requested location $LIBSODIUM_SEARCH_LIBS])
            ]
        )
    else
        AC_CHECK_LIB(sodium, randombytes_random,
            [],
            [
                AC_MSG_ERROR([required library libsodium was not found on your system, please check http://download.libsodium.org/libsodium/releases/])
            ]
        )
    fi

    LDFLAGS="$LDFLAGS_SAVE"
    AC_SUBST(LIBSODIUM_LIBS)
    AC_SUBST(LIBSODIUM_LDFLAGS)



    LIBTOXCORE_CFLAGS=
    CFLAGS_SAVE="$CFLAGS"
    CPPFLAGS_SAVE="$CPPFLAGS"

    if test -n "$LIBTOXCORE_SEARCH_HEADERS"; then
        CFLAGS="$CFLAGS -I$LIBTOXCORE_SEARCH_HEADERS"
        CPPFLAGS="$CPPFLAGS -I$LIBTOXCORE_SEARCH_HEADERS"
        AC_CHECK_HEADER([tox/tox.h],
            [
                LIBTOXCORE_CFLAGS="-I$LIBTOXCORE_SEARCH_HEADERS"
            ],
            [
                AC_MSG_ERROR([headers for the toxcore library were not found on your system])
            ]
        )
    else
        AC_CHECK_HEADER([tox/tox.h],
            [],
            [
                AC_MSG_ERROR([headers for the toxcore library were not found on your system])
            ],
        )
    fi
    CFLAGS="$CFLAGS_SAVE"
    CPPFLAGS="$CPPFLAGS_SAVE"
    AC_SUBST(LIBTOXCORE_CFLAGS)

    LIBTOXCORE_LIBS=
    LIBTOXCORE_LDFLAGS=
    LDFLAGS_SAVE="$LDFLAGS"
    if test -n "$LIBTOXCORE_SEARCH_LIBS"; then
        LDFLAGS="$LDFLAGS $LIBSODIUM_LDFLAGS -L$LIBTOXCORE_SEARCH_LIBS"
        AC_CHECK_LIB([toxcore], [tox_new],
            [
                LIBTOXCORE_LDFLAGS="-L$LIBTOXCORE_SEARCH_LIBS"
                LIBTOXCORE_LIBS="-ltoxcore"
            ],
            [
                AC_MSG_ERROR([required library toxcore was not found on your system])
            ],
            [
                $WINSOCK2_LIBS
                $LIBSODIUM_LIBS
            ]
        )
    else
        LDFLAGS="$LDFLAGS $LIBSODIUM_LDFLAGS"
        AC_CHECK_LIB([toxcore], [tox_new],
            [],
            [
                AC_MSG_ERROR([required library toxcore was not found on your system])
            ],
            [
                $WINSOCK2_LIBS
                $LIBSODIUM_LIBS
            ]
        )
    fi
    LDFLAGS="$LDFLAGS_SAVE"
    AC_SUBST(LIBTOXCORE_LIBS)
    AC_SUBST(LIBTOXCORE_LDFLAGS)
fi


####
#### A/V Stuff

AV_SEARCH_DIR=
BUILD_AV="yes"

AC_ARG_WITH(libtoxav-prefix,
    AC_HELP_STRING([--with-libtoxav-prefix=DIR],
                   [search for libtoxav in DIR, i.e. look for libraries in
                    DIR/lib and for headers in DIR/include]),
    [
        AV_SEARCH_DIR="$withval"
    ]
)

if test -n "$AV_SEARCH_DIR"; then
    CFLAGS="$CFLAGS -I$AV_SEARCH_DIR/include"
    CPPFLAGS="$CPPFLAGS -I$AV_SEARCH_DIR/include"
    LDFLAGS="$LDFLAGS -L$AV_SEARCH_DIR/lib"
    export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$AV_SEARCH_DIR/lib/pkgconfig
fi

# Check if specified enable
AC_ARG_ENABLE([av],
    [AC_HELP_STRING([--disable-av], [build AV support libraries (default: auto)]) ],
    [
        if test "x$enableval" = "xno"; then
            BUILD_AV="no"
        elif test "x$enableval" = "xyes"; then
            BUILD_AV="yes"
        fi
    ]
)

# Check for A/V library

if test "x$BUILD_AV" = "xyes"; then
    PKG_CHECK_MODULES([OPENAL], [openal],
    [        
        export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig

        PKG_CHECK_MODULES([LIBTOXAV], [libtoxav], 
        [
            AC_CHECK_HEADER([tox/toxav.h],
            [
                # Place define for audio support
                AC_DEFINE([_SUPPORT_AUDIO], [], [Is audio supported])
                AC_MSG_NOTICE([Building with audio support])
            ],
            [
                AC_MSG_NOTICE([No A/V headers; disabling A/V support])
                BUILD_AV="no"
            ],)
        ], 
        [
            AC_MSG_NOTICE([No A/V library; disabling A/V support])
            BUILD_AV="no"
        ])
    ],
    [
        AC_MSG_NOTICE([No openal library; disabling A/V support])
        BUILD_AV="no"
    ])
fi

AM_CONDITIONAL(BUILD_AV, test "x$BUILD_AV" = "xyes")


TOXIC_VERSION="$PACKAGE_VERSION"
AC_PATH_PROG([GIT], [git], [no])
if test "x$GIT" != "xno"; then
    if test -d ${srcdir}/.git; then
        TOXIC_VERSION="${TOXIC_VERSION}_r`${GIT} rev-list HEAD --count`"
    fi
fi
AC_SUBST(TOXIC_VERSION)

eval PACKAGE_DATADIR="${datadir}/${PACKAGE}"
eval PACKAGE_DATADIR="${PACKAGE_DATADIR}"
AC_DEFINE_UNQUOTED(PACKAGE_DATADIR, "$PACKAGE_DATADIR", [toxic data directory])

if test "x$NCURSES_WIDECHAR_SUPPORT" = "xyes"; then
    AC_DEFINE([HAVE_WIDECHAR], [1], [ncurses wide char support available])
    AC_DEFINE([_XOPEN_SOURCE_EXTENDED], [1],
              [enable X/Open Portability Guide functionality])
fi
 
AC_CONFIG_FILES([Makefile
                 misc/Makefile
                 build/Makefile])
AC_OUTPUT
