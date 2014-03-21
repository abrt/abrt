INTERFACES_DIST_FILES=$(wildcard *.xml.in)

%.xml: %.xml.in
	cp $< $@

dbusabrtinterfacesdir = ${dbusinterfacedir}
dist_dbusabrtinterfaces_DATA = \
	com.redhat.problems.configuration.xml \
	com.redhat.problems.configuration.abrt.xml \
	com.redhat.problems.configuration.ccpp.xml \
	com.redhat.problems.configuration.oops.xml \
	com.redhat.problems.configuration.python.xml \
	com.redhat.problems.configuration.vmcore.xml \
	com.redhat.problems.configuration.xorg.xml

EXTRA_DIST = ${INTERFACES_DIST_FILES}
