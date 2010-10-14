AC_INIT([btparser], [0.7], [kklic@redhat.com])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_MAINTAINER_MODE

AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_CC

# Initialize the test suite.
AC_CONFIG_TESTDIR(tests)
AC_CONFIG_FILES([tests/Makefile tests/atlocal])
AM_MISSING_PROG([AUTOM4TE], [autom4te])
# Needed by tests/atlocal.in.
AC_SUBST([O0CFLAGS], [`echo $CFLAGS   | sed 's/-O[[0-9]] *//'`])

AC_CONFIG_FILES([
	btparser.spec
	btparser.pc
	Makefile
	lib/Makefile
])

AC_OUTPUT
