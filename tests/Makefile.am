## ------------ ##
## package.m4.  ##
## ------------ ##

package.m4: Makefile.in
	{ \
             echo '# Signature of the current package.' && \
             echo 'm4_define([AT_PACKAGE_NAME],' && \
             echo '  [@PACKAGE_NAME@])' && \
             echo 'm4_define([AT_PACKAGE_TARNAME],' && \
             echo '  [@PACKAGE_TARNAME@])' && \
             echo 'm4_define([AT_PACKAGE_VERSION],' && \
             echo '  [@PACKAGE_VERSION@])' && \
             echo 'm4_define([AT_PACKAGE_STRING],' && \
             echo '  [@PACKAGE_STRING@])' && \
             echo 'm4_define([AT_PACKAGE_BUGREPORT],' && \
             echo '  [@PACKAGE_BUGREPORT@])'; \
             echo 'm4_define([AT_PACKAGE_URL],' && \
             echo '  [@PACKAGE_URL@])'; \
	} >'package.m4'
EXTRA_DIST = package.m4 ignored_problems_data

## ------------ ##
## Test suite.  ##
## ------------ ##

TESTSUITE_AT = \
  local.at \
  testsuite.at \
  pyhook.at \
  koops-parser.at \
  xorg-utils.at \
  ignored_problems.at \
  hooklib.at

EXTRA_DIST += $(TESTSUITE_AT)
TESTSUITE = $(srcdir)/testsuite
MAINTAINERCLEANFILES = Makefile.in $(TESTSUITE)
check_DATA = atconfig atlocal $(TESTSUITE)
DISTCLEANFILES = atconfig
EXTRA_DIST += atlocal.in
EXTRA_DIST += koops-test.h
EXTRA_DIST += GList_append.supp

atconfig: $(top_builddir)/config.status
	(cd ${top_builddir} && ./config.status ${subdir}/atconfig)

check-local: $(check_DATA)
	$(SHELL) '$(TESTSUITE)' $(TESTSUITEFLAGS)

.PHONY: maintainer-check-valgrind
maintainer-check-valgrind: $(check_DATA)
	$(MAKE) check-local \
		PRE_AT_CHECK='$(abs_top_builddir)/libtool --mode=execute $(VALGRIND)' \
		VALGRIND_OPTS='--tool=memcheck --leak-check=full --show-reachable=yes --error-exitcode=1 --suppressions=$(abs_top_builddir)/tests/GList_append.supp -q'

.PHONY: maintainer-check
maintainer-check: maintainer-check-valgrind

installcheck-local: $(check_DATA)
	$(SHELL) '$(TESTSUITE)' AUTOTEST_PATH='$(bindir)' $(TESTSUITEFLAGS)

clean-local:
	test ! -f '$(TESTSUITE)' || $(SHELL) '$(TESTSUITE)' --clean

AUTOTEST = $(AUTOM4TE) --language=autotest
$(TESTSUITE): $(TESTSUITE_AT) $(srcdir)/package.m4
	$(AUTOTEST) -I '$(srcdir)' -o $@.tmp $@.at
	mv $@.tmp $@
