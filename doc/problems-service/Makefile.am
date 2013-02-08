SPEC = org.freedesktop.Problems.xml

all: html/index.html

$(SPEC): $(SPEC).in
	cp $< $@

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
	xsltproc tools/spec-to-docbook.xsl $(SPEC) > $@

dbusabrtinterfacesdir = ${dbusinterfacedir}
dist_dbusabrtinterfaces_DATA = $(SPEC)

dbusabrtdocdir = ${datadir}/doc/abrt-dbus-${VERSION}/html
dist_dbusabrtdoc_DATA = html/style.css
dbusabrtdoc_DATA = html/index.html \
				   html/ch01.html \
				   html/ch02.html \
				   html/pt01.html \
				   html/re01.html

EXTRA_DIST = specification.xml.in \
	$(SPEC).in \
	docbook-params.xsl \
	html \
	html/style.css \
	tools \
	tools/*

CLEANFILES = reference.xml html/*.html html/specification.proc
