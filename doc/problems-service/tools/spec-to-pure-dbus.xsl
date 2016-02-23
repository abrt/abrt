<?xml version="1.0"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:tp="http://telepathy.freedesktop.org/wiki/DbusSpec#extensions-v0"
  xmlns:html="http://www.w3.org/1999/xhtml"
  xmlns:xlink="http://www.w3.org/1999/xlink"
  exclude-result-prefixes="tp html">

  <xsl:output
    doctype-public="-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
    doctype-system="http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd"
    cdata-section-elements="style"/>

  <xsl:output method="xml" indent="yes" encoding="ascii"
    omit-xml-declaration="yes"/>

  <xsl:strip-space elements="*"/>

  <xsl:param name="interface"/>

  <xsl:template match="tp:spec">
    <xsl:apply-templates select="node[interface[@name=$interface]]" />
  </xsl:template>

  <xsl:template match="node()|@*">
    <xsl:copy>
      <xsl:apply-templates select="node()|@*"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="tp:*"/>

  <xsl:template match="comment()"/>

</xsl:stylesheet>

<!-- vim:set sw=2 sts=2 et: -->
