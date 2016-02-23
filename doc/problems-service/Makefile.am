SPEC = org.freedesktop.Problems.xml.in
NS = org.freedesktop.Problems2

all: html/index.html

$(NS).xml: tools/spec-to-pure-dbus.xsl $(SPEC)
	xsltproc --stringparam interface $$(basename $@ .xml) $^ > $@

$(NS).Entry.xml: tools/spec-to-pure-dbus.xsl $(SPEC)
	xsltproc --stringparam interface $$(basename $@ .xml) $^ > $@

$(NS).Session.xml: tools/spec-to-pure-dbus.xsl $(SPEC)
	xsltproc --stringparam interface $$(basename $@ .xml) $^ > $@

$(NS).Task.xml: tools/spec-to-pure-dbus.xsl $(SPEC)
	xsltproc --stringparam interface $$(basename $@ .xml) $^ > $@

specification.xml: specification.xml.in
	cp $< $@

# Don't use 'xmlto xhtml' -> https://plus.google.com/115547683951727699051/posts/bigvpEke9PN
html/index.html: reference.xml docbook-params.xsl specification.xml
	xmlto html --skip-validation -o html/ -x docbook-params.xsl specification.xml

html/ch01.html: html/index.html
html/ch02.html: html/index.html
html/pt01.html: html/index.html
html/re01.html: html/index.html

reference.xml: tools/spec-to-docbook.xsl $(SPEC)
	xsltproc $^ > $@

dbusabrtinterfacesdir = ${dbusinterfacedir}
dist_dbusabrtinterfaces_DATA = $(NS).xml $(NS).Entry.xml $(NS).Session.xml $(NS).Task.xml

dbusabrtdocdir = ${datadir}/doc/abrt-dbus-${VERSION}/html
dist_dbusabrtdoc_DATA = html/style.css
dbusabrtdoc_DATA = html/index.html \
				   html/ch01.html \
				   html/ch02.html \
				   html/pt01.html \
				   html/re01.html

EXTRA_DIST = specification.xml.in \
	$(SPEC) \
	docbook-params.xsl \
	html \
	html/style.css \
	tools \
	tools/*

CLEANFILES = $(dist_dbusabrtinterfaces_DATA) reference.xml html/*.html html/specification.proc
