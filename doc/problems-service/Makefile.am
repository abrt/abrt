SPEC = org.freedesktop.Problems.xml

all: html/index.html

$(SPEC): $(SPEC).in
	cp $< $@

specification.xml: specification.xml.in
	cp $< $@

# Don't use 'xmlto xhtml' -> https://plus.google.com/115547683951727699051/posts/bigvpEke9PN
html/index.html: reference.xml docbook-params.xsl specification.xml
	xmlto html --skip-validation -o html/ -x docbook-params.xsl specification.xml

reference.xml: tools/spec-to-docbook.xsl $(SPEC)
	xsltproc tools/spec-to-docbook.xsl $(SPEC) > $@

EXTRA_DIST = specification.xml.in \
	$(SPEC).in \
	docbook-params.xsl \
	html \
	html/style.css \
	tools \
	tools/*

CLEANFILES = reference.xml html/*.html html/specification.proc
