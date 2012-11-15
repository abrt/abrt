<?xml version="1.0"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:tp="http://telepathy.freedesktop.org/wiki/DbusSpec#extensions-v0"
  xmlns:html="http://www.w3.org/1999/xhtml"
  xmlns:xlink="http://www.w3.org/1999/xlink"
  xmlns:docbook="http://docbook.org/ns/docbook"
  exclude-result-prefixes="tp html">

<!--
    Telepathy D-Bus Introspection to Docbook XML translator.
    Based on Telepathy's doc-generator.xsl.

    Copyright (C) 2006-2008 Collabora Limited
    Copyright (C) 2009 Michael Leupold <lemma@confuego.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
-->

  <xsl:output method="xml" indent="yes" encoding="ascii"
    omit-xml-declaration="no"/>

  <xsl:include href="resolve-type.xsl"/>

  <xsl:param name="allow-undefined-interfaces" select="false()"/>

  <xsl:template match="docbook:* | html:* | @*">
    <xsl:copy>
      <xsl:apply-templates/>
    </xsl:copy>
  </xsl:template>

  <xsl:template name="direction">
    <xsl:param name="indirection"/>
    <xsl:choose>
      <xsl:when test="$indirection = 'in'">IN</xsl:when>
      <xsl:otherwise>OUT</xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="tp:type">
    <xsl:call-template name="tp-type">
      <xsl:with-param name="tp-type" select="string(.)"/>
    </xsl:call-template>
  </xsl:template>

  <!-- tp:dbus-ref: reference a D-Bus interface, signal, method or property -->
  <xsl:template match="tp:dbus-ref">
    <xsl:variable name="name">
      <xsl:choose>
        <xsl:when test="@namespace">
          <xsl:value-of select="@namespace"/>
          <xsl:text>.</xsl:text>
        </xsl:when>
      </xsl:choose>
      <xsl:value-of select="string(.)"/>
    </xsl:variable>

    <xsl:choose>
      <xsl:when test="//interface[@name=$name]
        or //interface/method[concat(../@name, '.', @name)=$name]
        or //interface/signal[concat(../@name, '.', @name)=$name]
        or //interface/property[concat(../@name, '.', @name)=$name]
        or //interface[@name=concat($name, '.DRAFT')]
        or //interface/method[
          concat(../@name, '.', @name)=concat($name, '.DRAFT')]
        or //interface/signal[
          concat(../@name, '.', @name)=concat($name, '.DRAFT')]
        or //interface/property[
          concat(../@name, '.', @name)=concat($name, '.DRAFT')]
        ">
        <link linkend="{$name}">
          <literal><xsl:value-of select="$name"/></literal>
        </link>
      </xsl:when>

      <xsl:when test="$allow-undefined-interfaces">
        <!-- TODO: Convert to docbook -->
        <span xmlns="http://www.w3.org/1999/xhtml" title="defined elsewhere">
          <xsl:value-of select="string(.)"/>
        </span>
      </xsl:when>

      <xsl:otherwise>
        <xsl:message terminate="yes">
          <xsl:text>ERR: cannot find D-Bus interface, method, </xsl:text>
          <xsl:text>signal or property called '</xsl:text>
          <xsl:value-of select="$name"/>
          <xsl:text>'&#10;</xsl:text>
        </xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- tp:member-ref: reference a property of the current interface -->
  <xsl:template match="tp:member-ref">
    <xsl:variable name="prefix" select="concat(ancestor::interface/@name,
      '.')"/>
    <xsl:variable name="name" select="string(.)"/>

    <xsl:if test="not(ancestor::interface)">
      <xsl:message terminate="yes">
        <xsl:text>ERR: Cannot use tp:member-ref when not in an</xsl:text>
        <xsl:text> &lt;interface&gt;&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:choose>
      <xsl:when test="ancestor::interface/signal[@name=$name]"/>
      <xsl:when test="ancestor::interface/method[@name=$name]"/>
      <xsl:when test="ancestor::interface/property[@name=$name]"/>
      <xsl:otherwise>
        <xsl:message terminate="yes">
          <xsl:text>ERR: interface </xsl:text>
          <xsl:value-of select="ancestor::interface/@name"/>
          <xsl:text> has no signal/method/property called </xsl:text>
          <xsl:value-of select="$name"/>
          <xsl:text>&#10;</xsl:text>
        </xsl:message>
      </xsl:otherwise>
    </xsl:choose>

    <link linkend="{$prefix}{$name}">
      <literal><xsl:value-of select="concat($prefix, $name)"/></literal>
    </link>
  </xsl:template>

  <xsl:template match="*" mode="identity">
    <xsl:copy>
      <xsl:apply-templates mode="identity"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="tp:docstring">
    <para>
      <xsl:copy-of select="child::node()"/>
    </para>
  </xsl:template>

  <xsl:template match="tp:docstring" mode="nopara">
    <xsl:copy-of select="child::node()"/>
  </xsl:template>

  <xsl:template match="tp:added">
    <para>
      Added in version <xsl:value-of select="@version"/>.
      <xsl:apply-templates select="node()"/>
    </para>
  </xsl:template>

  <xsl:template match="tp:changed">
    <xsl:choose>
      <xsl:when test="node()">
        <para>
          Changed in version <xsl:value-of select="@version"/>:
          <xsl:apply-templates select="node()"/></para>
      </xsl:when>
      <xsl:otherwise>
        <para>Changed in version
          <xsl:value-of select="@version"/></para>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="tp:deprecated">
    <para>
      Deprecated since version <xsl:value-of select="@version"/>.
      <xsl:apply-templates select="node()"/>
    </para>
  </xsl:template>

  <xsl:template match="tp:rationale">
    <!-- TODO: special? -->
    <para>
      <xsl:apply-templates select="node()"/>
    </para>
  </xsl:template>

  <xsl:template match="tp:errors">
    <title>Errors</title>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template name="generic-types">
    <chapter>
      <xsl:attribute name="xml:id">types</xsl:attribute>
        <title>Types</title>
        <xsl:call-template name="do-types"/>
    </chapter>
  </xsl:template>

  <xsl:template name="do-types">
    <xsl:if test="tp:simple-type">
      <section>
        <title>Simple types</title>
        <xsl:apply-templates select="tp:simple-type"/>
      </section>
    </xsl:if>

    <xsl:if test="tp:enum">
      <section>
        <title>Enumerated types</title>
        <xsl:apply-templates select="tp:enum"/>
      </section>
    </xsl:if>

    <xsl:if test="tp:flags">
      <section>
        <title>Sets of flags</title>
        <xsl:apply-templates select="tp:flags"/>
      </section>
    </xsl:if>

    <xsl:if test="tp:struct">
      <section>
        <title>Struct types</title>
        <xsl:apply-templates select="tp:struct"/>
      </section>
    </xsl:if>

    <xsl:if test="tp:mapping">
      <section>
        <title>Map types</title>
        <xsl:apply-templates select="tp:mapping"/>
      </section>
    </xsl:if>

    <xsl:if test="tp:external-type">
      <section>
        <title>Types defined elsewhere</title>
        <glosslist>
          <xsl:apply-templates select="tp:external-type"/>
        </glosslist>
      </section>
    </xsl:if>
  </xsl:template>

  <xsl:template match="tp:error">
    <simplesect>
      <title>
        <xsl:attribute name="xml:id">
          <xsl:value-of select="concat(../@namespace, '.', translate(@name, ' ', ''))"/>
        </xsl:attribute>
        <literal><xsl:value-of select="concat(../@namespace, '.', translate(@name, ' ', ''))"/></literal>
      </title>
      <xsl:apply-templates select="tp:docstring"/>
      <xsl:apply-templates select="tp:added"/>
      <xsl:apply-templates select="tp:changed"/>
      <xsl:apply-templates select="tp:deprecated"/>
    </simplesect>
  </xsl:template>

  <xsl:template match="/tp:spec/tp:copyright">
    <!-- TODO: use <copyright> -->
    <legalnotice>
      <para><xsl:apply-templates mode="text"/></para>
    </legalnotice>
  </xsl:template>
  <xsl:template match="/tp:spec/tp:license">
    <!-- TODO: right tag? -->
    <legalnotice>
      <para>
        <xsl:apply-templates/>
      </para>
    </legalnotice>
  </xsl:template>

  <xsl:template match="tp:copyright"/>
  <xsl:template match="tp:license"/>

  <xsl:template match="interface">

    <refentry>
      <xsl:attribute name="xml:id">
        <xsl:value-of select="@name"/>
      </xsl:attribute>
      <refmeta>
        <refentrytitle><literal><xsl:value-of select="@name"/></literal></refentrytitle>
      </refmeta>

      <refnamediv>
        <refdescriptor><xsl:value-of select="@name"/></refdescriptor>
        <refname><xsl:value-of select="@name"/></refname>
        <refpurpose><xsl:apply-templates select="tp:docstring" mode="nopara"/></refpurpose>
      </refnamediv>

      <xsl:if test="tp:added">
        <refsection>
          <xsl:apply-templates select="tp:added"/>
        </refsection>
      </xsl:if>
      <xsl:if test="tp:changed">
        <refsection>
          <xsl:apply-templates select="tp:changed"/>
        </refsection>
      </xsl:if>
      <xsl:if test="tp:deprecated">
        <refsection>
          <xsl:apply-templates select="tp:deprecated"/>
        </refsection>
      </xsl:if>

      <xsl:if test="@tp:causes-havoc">
        <refsection>
          <warning>
            <para>
              This interface is <xsl:value-of select="@tp:causes-havoc"/>
              and is likely to cause havoc to your API/ABI if bindings are generated.
              Don't include it in libraries that care about compatibility.
            </para>
          </warning>
        </refsection>
      </xsl:if>

      <xsl:if test="tp:requires">
        <refsection>
          <tip>
            <para>Implementations of this interface must also implement:</para>
            <itemizedlist>
              <xsl:for-each select="tp:requires">
                <listitem>
                  <para>
                    <link linkend="{@interface}">
                      <literal><xsl:value-of select="@interface"/></literal>
                    </link>
                  </para>
                </listitem>
              </xsl:for-each>
            </itemizedlist>
          </tip>
        </refsection>
      </xsl:if>

      <refsynopsisdiv>
        <xsl:if test="method">
          <refsect2>
            <title>Methods</title>
            <funcsynopsis>
              <xsl:apply-templates select="method" mode="funcsynopsislinked"/>
            </funcsynopsis>
          </refsect2>
        </xsl:if>
        <xsl:if test="signal">
          <refsect2>
            <title>Signals</title>
            <funcsynopsis>
              <xsl:apply-templates select="signal" mode="funcsynopsislinked"/>
            </funcsynopsis>
          </refsect2>
        </xsl:if>
        <xsl:if test="property">
          <refsect2>
            <title>Properties</title>
            <refsynopsisdiv>
              <title> </title>
              <xsl:apply-templates select="property" mode="fieldsynopsislinked"/>
            </refsynopsisdiv>
          </refsect2>
        </xsl:if>
      </refsynopsisdiv>

      <xsl:if test="method">
        <refsection>
          <title>Methods</title>
          <xsl:apply-templates select="method" mode="detail"/>
        </refsection>
      </xsl:if>

      <xsl:if test="signal">
        <refsection>
          <title>Signals</title>
          <xsl:apply-templates select="signal" mode="detail"/>
        </refsection>
      </xsl:if>

      <xsl:if test="tp:property">
        <refsection>
          <title>Telepathy Properties</title>
          <para>
            Accessed using the
            <link linkend="org.freedesktop.Telepathy.Properties">
              <literal>org.freedesktop.Telepathy.Properties</literal>
            </link>
          </para>
          <glosslist>
            <xsl:apply-templates select="tp:property" mode="detail"/>
          </glosslist>
        </refsection>
      </xsl:if>

      <xsl:if test="property">
        <refsection>
          <title>D-Bus Properties</title>
          <para>
            Accessed using the org.freedesktop.DBus.Properties interface.
          </para>
          <xsl:apply-templates select="property" mode="detail"/>
        </refsection>
      </xsl:if>

      <xsl:call-template name="do-types"/>

    </refentry>

  </xsl:template>

  <xsl:template match="tp:flags">

    <xsl:if test="not(@name) or @name = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @name on a tp:flags type&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@type) or @type = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @type on tp:flags type</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <section>
      <xsl:attribute name="xml:id">
        <xsl:value-of select="@name"/>
      </xsl:attribute>
      <title><literal><xsl:value-of select="@name"/></literal></title>
      <xsl:apply-templates select="tp:docstring" />
      <xsl:apply-templates select="tp:added"/>
      <xsl:apply-templates select="tp:changed"/>
      <xsl:apply-templates select="tp:deprecated"/>
      <glosslist>
        <xsl:variable name="value-prefix">
          <xsl:choose>
            <xsl:when test="@value-prefix">
              <xsl:value-of select="@value-prefix"/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="@name"/>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
        <xsl:for-each select="tp:flag">
          <glossentry>
            <glossterm>
              <xsl:value-of select="concat($value-prefix, '_', @suffix)"/> = <xsl:value-of select="@value"/>
            </glossterm>
            <glossdef>
              <xsl:choose>
                <xsl:when test="tp:docstring">
                  <xsl:apply-templates select="tp:docstring" />
                  <xsl:apply-templates select="tp:added"/>
                  <xsl:apply-templates select="tp:changed"/>
                  <xsl:apply-templates select="tp:deprecated"/>
                </xsl:when>
                <xsl:otherwise>
                  (Undocumented)
                </xsl:otherwise>
              </xsl:choose>
            </glossdef>
          </glossentry>
        </xsl:for-each>
      </glosslist>
    </section>
  </xsl:template>

  <xsl:template match="tp:enum">

    <xsl:if test="not(@name) or @name = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @name on a tp:enum type&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@type) or @type = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @type on tp:enum type</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <section>
      <xsl:attribute name="xml:id">
        <xsl:value-of select="concat('type-', @name)"/>
      </xsl:attribute>
      <title><literal><xsl:value-of select="@name"/></literal></title>
      <xsl:apply-templates select="tp:docstring" />
      <xsl:apply-templates select="tp:added"/>
      <xsl:apply-templates select="tp:changed"/>
      <xsl:apply-templates select="tp:deprecated"/>
      <glosslist>
        <xsl:variable name="value-prefix">
          <xsl:choose>
            <xsl:when test="@value-prefix">
              <xsl:value-of select="@value-prefix"/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="@name"/>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
        <xsl:for-each select="tp:enumvalue">
          <glossentry>
            <glossterm>
              <xsl:value-of select="concat($value-prefix, '_', @suffix)"/> = <xsl:value-of select="@value"/>
            </glossterm>
            <glossdef>
              <xsl:choose>
                <xsl:when test="tp:docstring">
                  <xsl:apply-templates select="tp:docstring" />
                  <xsl:apply-templates select="tp:added"/>
                  <xsl:apply-templates select="tp:changed"/>
                  <xsl:apply-templates select="tp:deprecated"/>
                </xsl:when>
                <xsl:otherwise>
                  (Undocumented)
                </xsl:otherwise>
              </xsl:choose>
            </glossdef>
          </glossentry>
        </xsl:for-each>
      </glosslist>
    </section>
  </xsl:template>

  <xsl:template match="property" mode="fieldsynopsis">

    <fieldsynopsis>
      <xsl:attribute name="xml:id">
        <xsl:value-of select="concat(../@name, '.', @name)"/>
      </xsl:attribute>
      <modifier>
        <xsl:choose>
          <xsl:when test="@access = 'read'">
            <xsl:text>READ</xsl:text>
          </xsl:when>
          <xsl:when test="@access = 'write'">
            <xsl:text>WRITE</xsl:text>
          </xsl:when>
          <xsl:when test="@access = 'readwrite'">
            <xsl:text>READWRITE</xsl:text>
          </xsl:when>
          <xsl:otherwise>
            <xsl:message terminate="yes">
              <xsl:text>ERR: unknown or missing value for </xsl:text>
              <xsl:text>@access on property </xsl:text>
              <xsl:value-of select="concat(../@name, '.', @name)"/>
              <xsl:text>: '</xsl:text>
              <xsl:value-of select="@access"/>
              <xsl:text>'&#10;</xsl:text>
            </xsl:message>
          </xsl:otherwise>
        </xsl:choose>
      </modifier>
      <type>
        <xsl:call-template name="ResolveType">
          <xsl:with-param name="node" select="."/>
        </xsl:call-template>
      </type>
      <varname>
        <xsl:value-of select="@name"/>
      </varname>
    </fieldsynopsis>

  </xsl:template>

  <xsl:template match="property" mode="fieldsynopsislinked">

    <fieldsynopsis>
      <modifier>
        <xsl:choose>
          <xsl:when test="@access = 'read'">
            <xsl:text>READ</xsl:text>
          </xsl:when>
          <xsl:when test="@access = 'write'">
            <xsl:text>WRITE</xsl:text>
          </xsl:when>
          <xsl:when test="@access = 'readwrite'">
            <xsl:text>READWRITE</xsl:text>
          </xsl:when>
          <xsl:otherwise>
            <xsl:message terminate="yes">
              <xsl:text>ERR: unknown or missing value for </xsl:text>
              <xsl:text>@access on property </xsl:text>
              <xsl:value-of select="concat(../@name, '.', @name)"/>
              <xsl:text>: '</xsl:text>
              <xsl:value-of select="@access"/>
              <xsl:text>'&#10;</xsl:text>
            </xsl:message>
          </xsl:otherwise>
        </xsl:choose>
      </modifier>
      <type>
        <xsl:call-template name="ResolveType">
          <xsl:with-param name="node" select="."/>
        </xsl:call-template>
      </type>
      <varname>
        <xsl:attribute name="xlink:href">
          <xsl:value-of select="concat('#', ../@name, '.', @name)"/>
        </xsl:attribute>
        <xsl:value-of select="@name"/>
      </varname>
    </fieldsynopsis>

  </xsl:template>

  <xsl:template match="property" mode="detail">

    <xsl:if test="not(parent::interface)">
      <xsl:message terminate="yes">
        <xsl:text>ERR: property </xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text> does not have an interface as parent&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@name) or @name = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @name on a property of </xsl:text>
        <xsl:value-of select="../@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@type) or @type = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @type on property </xsl:text>
        <xsl:value-of select="concat(../@name, '.', @name)"/>
        <xsl:text>: '</xsl:text>
        <xsl:value-of select="@access"/>
        <xsl:text>'&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:apply-templates select="." mode="fieldsynopsis"/>

    <xsl:apply-templates select="tp:docstring"/>
    <xsl:apply-templates select="tp:added"/>
    <xsl:apply-templates select="tp:changed"/>
    <xsl:apply-templates select="tp:deprecated"/>
  </xsl:template>

  <xsl:template match="tp:property" mode="detail">
    <glossentry>
      <glossterm>
        <xsl:if test="@name">
          <xsl:value-of select="@name"/> −
        </xsl:if>
        <xsl:value-of select="@type"/>
      </glossterm>
      <glossdef>
        <xsl:apply-templates select="tp:docstring"/>
        <xsl:apply-templates select="tp:added"/>
        <xsl:apply-templates select="tp:changed"/>
        <xsl:apply-templates select="tp:deprecated"/>
      </glossdef>
    </glossentry>
  </xsl:template>

  <xsl:template match="tp:mapping">
    <section>
      <xsl:attribute name="xml:id">
        <xsl:value-of select="concat('type-', @name)"/>
      </xsl:attribute>
      <title>
        <literal><xsl:value-of select="@name"/></literal>
      </title>
      <xsl:apply-templates select="tp:docstring"/>
      <xsl:if test="string(@array-name) != ''">
        <para>
          In bindings that need a separate name, arrays of
          <xsl:value-of select="@name"/> should be called
          <xsl:value-of select="@array-name"/>.
        </para>
      </xsl:if>
      <section>
        <title>Members</title>
        <glosslist>
          <xsl:apply-templates select="tp:member" mode="description"/>
        </glosslist>
      </section>
    </section>
  </xsl:template>

  <xsl:template match="tp:docstring" mode="in-index"/>

  <xsl:template match="tp:simple-type | tp:enum | tp:flags | tp:external-type"
    mode="in-index">
    − <xsl:value-of select="@type"/>
  </xsl:template>

  <xsl:template match="tp:simple-type">

    <xsl:if test="not(@name) or @name = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @name on a tp:simple-type&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@type) or @type = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @type on tp:simple-type</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <section>
      <xsl:attribute name="xml:id">
        <xsl:value-of select="concat('type-', @name)"/>
      </xsl:attribute>
      <title>
        <literal><xsl:value-of select="@name"/> − <xsl:value-of select="@type"/></literal>
      </title>
      <para>
        <xsl:apply-templates select="tp:docstring"/>
        <xsl:apply-templates select="tp:added"/>
        <xsl:apply-templates select="tp:changed"/>
        <xsl:apply-templates select="tp:deprecated"/>
      </para>
    </section>
  </xsl:template>

  <xsl:template match="tp:external-type">

    <xsl:if test="not(@name) or @name = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @name on a tp:external-type&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@type) or @type = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @type on tp:external-type</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <glossentry>
      <glossterm>
        <xsl:attribute name="xml:id">
          <xsl:value-of select="concat('type-', @name)"/>
        </xsl:attribute>
        <xsl:value-of select="@name"/> − <xsl:value-of select="@type"/>
      </glossterm>
      <glossdef>Defined by: <xsl:value-of select="@from"/></glossdef>
    </glossentry>
  </xsl:template>

  <xsl:template match="tp:struct" mode="in-index">
    − ( <xsl:for-each select="tp:member">
          <xsl:value-of select="@type"/>
          <xsl:if test="position() != last()">, </xsl:if>
        </xsl:for-each> )
  </xsl:template>

  <xsl:template match="tp:mapping" mode="in-index">
    − a{ <xsl:for-each select="tp:member">
          <xsl:value-of select="@type"/>
          <xsl:if test="position() != last()"> &#x2192; </xsl:if>
        </xsl:for-each> }
  </xsl:template>

  <xsl:template match="tp:struct">
    <section>
      <xsl:attribute name="xml:id">
        <xsl:value-of select="concat('type-', @name)"/>
      </xsl:attribute>
      <title>
        <literal>
          <xsl:value-of select="@name"/>
        </literal>
      </title>
      <xsl:apply-templates select="tp:docstring"/>
      <xsl:apply-templates select="tp:added"/>
      <xsl:apply-templates select="tp:changed"/>
      <xsl:apply-templates select="tp:deprecated"/>
      <xsl:choose>
        <xsl:when test="string(@array-name) != ''">
          <para>In bindings that need a separate name, arrays of
            <xsl:value-of select="@name"/> should be called
            <xsl:value-of select="@array-name"/>.</para>
        </xsl:when>
        <xsl:otherwise>
          <para>Arrays of <xsl:value-of select="@name"/> don't generally
            make sense.</para>
        </xsl:otherwise>
      </xsl:choose>
      <classsynopsis>
        <ooclass>
          <modifier>struct</modifier>
          <classname><xsl:value-of select="@name"/></classname>
        </ooclass>
        <xsl:apply-templates select="tp:member" mode="fieldsynopsis"/>
      </classsynopsis>
      <glosslist>
        <xsl:apply-templates select="tp:member" mode="description"/>
      </glosslist>
    </section>
  </xsl:template>

  <xsl:template match="arg" mode="paramdef">
    <paramdef>
      <xsl:call-template name="direction">
        <xsl:with-param name="indirection" select="@direction"/>
      </xsl:call-template>
      <xsl:text> </xsl:text>
      <type>
        <xsl:call-template name="ResolveType">
          <xsl:with-param name="node" select="."/>
        </xsl:call-template>
      </type>
      <xsl:text> </xsl:text>
      <parameter><xsl:value-of select="@name"/></parameter>
    </paramdef>
  </xsl:template>

  <xsl:template match="arg" mode="paramtable">
    <glossentry>
      <glossterm><literal><xsl:value-of select="@name"/></literal></glossterm>
      <glossdef>
        <para><xsl:apply-templates select="tp:docstring" mode="nopara"/></para>
      </glossdef>
    </glossentry>
  </xsl:template>

  <xsl:template match="method|signal" mode="funcsynopsis">
    <funcsynopsis>
      <funcprototype>
        <funcdef>
          <function>
            <xsl:value-of select="@name"/>
          </function>
        </funcdef>
        <xsl:choose>
          <xsl:when test="arg">
            <xsl:apply-templates select="arg" mode="paramdef"/>
          </xsl:when>
          <xsl:otherwise>
            <void/>
          </xsl:otherwise>
        </xsl:choose>
      </funcprototype>
    </funcsynopsis>
  </xsl:template>

  <xsl:template match="method|signal" mode="funcsynopsislinked">
    <funcprototype>
      <funcdef>
        <function linkend="{concat(parent::interface//@name, '.', @name)}">
          <xsl:value-of select="@name"/>
        </function>
      </funcdef>
      <xsl:choose>
        <xsl:when test="arg">
          <xsl:apply-templates select="arg" mode="paramdef"/>
        </xsl:when>
        <xsl:otherwise>
          <void/>
        </xsl:otherwise>
      </xsl:choose>
    </funcprototype>
  </xsl:template>

  <xsl:template match="method" mode="detail">

    <xsl:if test="not(parent::interface)">
      <xsl:message terminate="yes">
        <xsl:text>ERR: method </xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text> does not have an interface as parent&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@name) or @name = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @name on a method of </xsl:text>
        <xsl:value-of select="../@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:for-each select="arg">
      <xsl:if test="not(@type) or @type = ''">
        <xsl:message terminate="yes">
          <xsl:text>ERR: an arg of method </xsl:text>
          <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
          <xsl:text> has no type</xsl:text>
        </xsl:message>
      </xsl:if>
      <xsl:choose>
        <xsl:when test="@direction='in'">
          <xsl:if test="not(@name) or @name = ''">
            <xsl:message terminate="yes">
              <xsl:text>ERR: an 'in' arg of method </xsl:text>
              <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
              <xsl:text> has no name</xsl:text>
            </xsl:message>
          </xsl:if>
        </xsl:when>
        <xsl:when test="@direction='out'">
          <xsl:if test="not(@name) or @name = ''">
            <xsl:message terminate="no">
              <xsl:text>INFO: an 'out' arg of method </xsl:text>
              <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
              <xsl:text> has no name</xsl:text>
            </xsl:message>
          </xsl:if>
        </xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes">
            <xsl:text>ERR: an arg of method </xsl:text>
            <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
            <xsl:text> has direction neither 'in' nor 'out'</xsl:text>
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>

    <refsection>
      <xsl:attribute name="xml:id">
        <xsl:value-of select="concat(../@name, concat('.', @name))"/>
      </xsl:attribute>
      <title>
        <literal><xsl:value-of select="concat(../@name, concat('.', @name))"/></literal>
      </title>
      <xsl:apply-templates select="." mode="funcsynopsis"/>
      <xsl:apply-templates select="tp:docstring"/>
      <xsl:apply-templates select="tp:added"/>
      <xsl:apply-templates select="tp:changed"/>
      <xsl:apply-templates select="tp:deprecated"/>

        <xsl:if test="arg">
          <glosslist>
            <xsl:apply-templates select="arg" mode="paramtable"/>
          </glosslist>
        </xsl:if>

        <xsl:if test="tp:possible-errors">
          <formalpara>
            <title>Possible errors</title>
            <para>
              <glosslist>
                <xsl:apply-templates select="tp:possible-errors/tp:error"/>
              </glosslist>
            </para>
          </formalpara>
        </xsl:if>
    </refsection>
  </xsl:template>

  <xsl:template name="tp-type">
    <xsl:param name="tp-type"/>
    <xsl:param name="type"/>

    <xsl:variable name="single-type">
      <xsl:choose>
        <xsl:when test="contains($tp-type, '[]')">
          <xsl:value-of select="substring-before($tp-type, '[]')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$tp-type"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:variable name="type-of-tp-type">
      <xsl:if test="contains($tp-type, '[]')">
        <!-- one 'a', plus one for each [ after the [], and delete all ] -->
        <xsl:value-of select="concat('a',
          translate(substring-after($tp-type, '[]'), '[]', 'a'))"/>
      </xsl:if>

      <xsl:choose>
        <xsl:when test="//tp:simple-type[@name=$single-type]">
          <xsl:value-of select="string(//tp:simple-type[@name=$single-type]/@type)"/>
        </xsl:when>
        <xsl:when test="//tp:struct[@name=$single-type]">
          <xsl:text>(</xsl:text>
          <xsl:for-each select="//tp:struct[@name=$single-type]/tp:member">
            <xsl:value-of select="@type"/>
          </xsl:for-each>
          <xsl:text>)</xsl:text>
        </xsl:when>
        <xsl:when test="//tp:enum[@name=$single-type]">
          <xsl:value-of select="string(//tp:enum[@name=$single-type]/@type)"/>
        </xsl:when>
        <xsl:when test="//tp:flags[@name=$single-type]">
          <xsl:value-of select="string(//tp:flags[@name=$single-type]/@type)"/>
        </xsl:when>
        <xsl:when test="//tp:mapping[@name=$single-type]">
          <xsl:text>a{</xsl:text>
          <xsl:for-each select="//tp:mapping[@name=$single-type]/tp:member">
            <xsl:value-of select="@type"/>
          </xsl:for-each>
          <xsl:text>}</xsl:text>
        </xsl:when>
        <xsl:when test="//tp:external-type[@name=$single-type]">
          <xsl:value-of select="string(//tp:external-type[@name=$single-type]/@type)"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes">
            <xsl:text>ERR: Unable to find type '</xsl:text>
            <xsl:value-of select="$tp-type"/>
            <xsl:text>'&#10;</xsl:text>
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:if test="string($type) != '' and
      string($type-of-tp-type) != string($type)">
      <xsl:message terminate="yes">
        <xsl:text>ERR: tp:type '</xsl:text>
        <xsl:value-of select="$tp-type"/>
        <xsl:text>' has D-Bus type '</xsl:text>
        <xsl:value-of select="$type-of-tp-type"/>
        <xsl:text>' but has been used with type='</xsl:text>
        <xsl:value-of select="$type"/>
        <xsl:text>'&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <link linkend="type-{$single-type}">
      <literal><xsl:value-of select="concat('type-', $single-type)"/></literal>
    </link>

  </xsl:template>

  <xsl:template name="parenthesized-tp-type">
    <xsl:if test="@tp:type">
      <xsl:text> (</xsl:text>
      <xsl:call-template name="tp-type">
        <xsl:with-param name="tp-type" select="@tp:type"/>
        <xsl:with-param name="type" select="@type"/>
      </xsl:call-template>
      <xsl:text>)</xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:template match="tp:member" mode="fieldsynopsis">
    <xsl:variable name="type">
      <xsl:call-template name="ResolveType">
        <xsl:with-param name="node" select="."/>
      </xsl:call-template>
    </xsl:variable>
    <fieldsynopsis>
      <type><xsl:value-of select="normalize-space($type)"/></type>
      <varname><xsl:value-of select="@name"/></varname>
    </fieldsynopsis>
  </xsl:template>

  <xsl:template match="tp:member" mode="description">
    <glossentry>
      <glossterm>
        <varname><xsl:value-of select="@name"/></varname>
      </glossterm>
      <glossdef>
        <xsl:choose>
          <xsl:when test="tp:docstring">
            <xsl:apply-templates select="tp:docstring" />
          </xsl:when>
          <xsl:otherwise>
            <!-- emphasize -->
            (undocumented)
          </xsl:otherwise>
        </xsl:choose>
      </glossdef>
    </glossentry>
  </xsl:template>

  <xsl:template match="tp:possible-errors/tp:error">
    <glossentry>
      <glossterm>
        <xsl:value-of select="@name"/>
      </glossterm>
      <glossdef>
        <xsl:variable name="name" select="@name"/>
        <xsl:choose>
          <xsl:when test="tp:docstring">
            <xsl:apply-templates select="tp:docstring"/>
          </xsl:when>
          <xsl:when test="//tp:errors/tp:error[concat(../@namespace, '.', translate(@name, ' ', ''))=$name]/tp:docstring">
            <xsl:apply-templates select="//tp:errors/tp:error[concat(../@namespace, '.', translate(@name, ' ', ''))=$name]/tp:docstring"/> <!-- TODO: emphasize -->(generic description)
          </xsl:when>
          <xsl:otherwise>
            (Undocumented.)
          </xsl:otherwise>
        </xsl:choose>
      </glossdef>
    </glossentry>
  </xsl:template>

  <xsl:template match="signal" mode="detail">

    <xsl:if test="not(parent::interface)">
      <xsl:message terminate="yes">
        <xsl:text>ERR: signal </xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text> does not have an interface as parent&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@name) or @name = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @name on a signal of </xsl:text>
        <xsl:value-of select="../@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:for-each select="arg">
      <xsl:if test="not(@type) or @type = ''">
        <xsl:message terminate="yes">
          <xsl:text>ERR: an arg of signal </xsl:text>
          <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
          <xsl:text> has no type</xsl:text>
        </xsl:message>
      </xsl:if>
      <xsl:if test="not(@name) or @name = ''">
        <xsl:message terminate="yes">
          <xsl:text>ERR: an arg of signal </xsl:text>
          <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
          <xsl:text> has no name</xsl:text>
        </xsl:message>
      </xsl:if>
      <xsl:choose>
        <xsl:when test="not(@direction)"/>
        <xsl:when test="@direction='in'"/>
        <!-- This doesn't work with the DTD (see comment in DTD)
        <xsl:when test="@direction='in'">
          <xsl:message terminate="no">
            <xsl:text>INFO: an arg of signal </xsl:text>
            <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
            <xsl:text> has unnecessary direction 'in'</xsl:text>
          </xsl:message>
        </xsl:when>
        -->
        <xsl:otherwise>
          <xsl:message terminate="yes">
            <xsl:text>ERR: an arg of signal </xsl:text>
            <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
            <xsl:text> has direction other than 'in'</xsl:text>
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>

    <refsection>
      <xsl:attribute name="xml:id">
        <xsl:value-of select="concat(../@name, concat('.', @name))"/>
      </xsl:attribute>
      <title>
        <literal><xsl:value-of select="concat(../@name, concat('.', @name))"/></literal>
      </title>
      <xsl:apply-templates select="." mode="funcsynopsis"/>
      <xsl:apply-templates select="tp:docstring"/>
      <xsl:apply-templates select="tp:added"/>
      <xsl:apply-templates select="tp:changed"/>
      <xsl:apply-templates select="tp:deprecated"/>

      <xsl:if test="arg">
        <glosslist>
          <xsl:apply-templates select="arg" mode="paramtable"/>
        </glosslist>
      </xsl:if>
    </refsection>
  </xsl:template>

  <xsl:template match="/tp:spec">
    <book xmlns="http://docbook.org/ns/docbook" version="5.0">
      <bookinfo>
        <title><xsl:value-of select="tp:title"/></title>
        <xsl:apply-templates select="tp:copyright"/>
        <xsl:apply-templates select="tp:license"/>
        <xsl:if test="tp:docstring">
          <abstract>
            <xsl:apply-templates select="tp:docstring"/>
          </abstract>
        </xsl:if>
        <!-- TODO: Version
        <xsl:if test="tp:version">
          <xsl:text> version </xsl:text>
          <xsl:value-of select="tp:version"/>
        </xsl:if> -->
      </bookinfo>
      <chapter>
        <xsl:attribute name="xml:id">interfaces</xsl:attribute>
        <title>Interfaces</title>
        <xsl:apply-templates select="//node"/>
      </chapter>
      <xsl:call-template name="generic-types"/>
      <xsl:if test="tp:errors">
        <chapter>
            <xsl:attribute name="xml:id">errors</xsl:attribute>
            <xsl:apply-templates select="tp:errors"/>
        </chapter>
      </xsl:if>
    </book>
  </xsl:template>

  <xsl:template match="node">
      <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="text()">
    <xsl:if test="normalize-space(.) != ''">
      <xsl:message terminate="yes">
        <xsl:text>Stray text: {{{</xsl:text>
        <xsl:value-of select="." />
        <xsl:text>}}}&#10;</xsl:text>
      </xsl:message>
    </xsl:if>
  </xsl:template>

  <xsl:template match="*">
      <xsl:message terminate="yes">
         <xsl:text>Unrecognised element: {</xsl:text>
         <xsl:value-of select="namespace-uri(.)" />
         <xsl:text>}</xsl:text>
         <xsl:value-of select="local-name(.)" />
         <xsl:text>&#10;</xsl:text>
      </xsl:message>
  </xsl:template>
</xsl:stylesheet>

<!-- vim:set sw=2 sts=2 et: -->
