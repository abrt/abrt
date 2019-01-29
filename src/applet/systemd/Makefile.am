systemduserunit_DATA = abrt-applet.service

abrt-applet.service: abrt-applet.service.in
	$(AM_V_GEN) sed -e "s|\@bindir\@|$(bindir)|" $< > $@

EXTRA_DIST = \
	abrt-applet.service.in
