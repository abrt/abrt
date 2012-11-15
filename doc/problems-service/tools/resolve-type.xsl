<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:tp="http://telepathy.freedesktop.org/wiki/DbusSpec#extensions-v0"
  xmlns:html="http://www.w3.org/1999/xhtml"
  exclude-result-prefixes="tp html">

<!--
    Helper templates for Telepathy D-Bus Introspection conversion.

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

    <!-- Resolve the type a node has. This will first look at tp:type and
        - if not found - use the type attribute -->
    <xsl:template name="ResolveType">
        <xsl:param name="node"/>
        <xsl:variable name="unstripped">
            <xsl:choose>
                <xsl:when test="$node//@tp:type">
                    <xsl:call-template name="TpType">
                        <xsl:with-param name="type" select="$node//@tp:type"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="$node//@type">
                    <xsl:call-template name="DBusType">
                        <xsl:with-param name="type" select="$node//@type"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:message terminate="yes">
                        Node doesn't contain a type.
                    </xsl:message>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:variable>
        <xsl:value-of select="translate(translate($unstripped, ' ', ''), '&#xa;', '')"/>
    </xsl:template>

    <!-- Map a D-Bus type to its EggDBus counterpart -->
    <xsl:template name="DBusType">
        <xsl:param name="type"/>
        <xsl:choose>
            <xsl:when test="$type='o'">ObjectPath</xsl:when>
            <xsl:when test="$type='s'">String</xsl:when>
            <xsl:when test="$type='y'">Byte</xsl:when>
            <xsl:when test="$type='b'">Boolean</xsl:when>
            <xsl:when test="$type='n'">Int16</xsl:when>
            <xsl:when test="$type='q'">UInt16</xsl:when>
            <xsl:when test="$type='i'">Int32</xsl:when>
            <xsl:when test="$type='u'">UInt32</xsl:when>
            <xsl:when test="$type='x'">Int64</xsl:when>
            <xsl:when test="$type='t'">UInt64</xsl:when>
            <xsl:when test="$type='d'">Double</xsl:when>
            <xsl:when test="$type='g'">Signature</xsl:when>
            <xsl:when test="$type='v'">Variant</xsl:when>
            <xsl:when test="starts-with($type, 'a{')">
                Dict&lt;
                <xsl:call-template name="DBusType">
                    <xsl:with-param name="type" select="substring($type, 3, 1)"/>
                </xsl:call-template>
                ,
                <xsl:call-template name="DBusType">
                    <xsl:with-param name="type" select="substring($type, 4, 1)"/>
                </xsl:call-template>
                &gt;
            </xsl:when>
            <xsl:when test="starts-with($type, 'a')">
                Array&lt;
                <xsl:call-template name="DBusType">
                    <xsl:with-param name="type" select="substring($type, 2)"/>
                </xsl:call-template>
                &gt;
            </xsl:when>
            <!-- TODO: doesn't implement dict-entries and structs -->
            <xsl:otherwise>
                <xsl:message terminate="yes">
                    Unknown DBus Type <xsl:value-of select="$type"/>
                </xsl:message>
            </xsl:otherwise>
        </xsl:choose>
    </xsl:template>

    <!-- Resolve tp:type attributes by searching for matching tp:struct
            and tp:mapping elements -->
    <xsl:template name="TpType">
        <xsl:param name="type"/>
        <xsl:choose>
            <xsl:when test="/tp:spec/tp:struct[@name=$type]">
                <xsl:value-of select="$type"/>
            </xsl:when>
            <xsl:when test="/tp:spec/tp:mapping[@name=$type]">
                Dict&lt;
                <xsl:call-template name="ResolveType">
                    <xsl:with-param name="node" select="/tp:spec/tp:mapping[@name=$type]/tp:member[@name='Key']"/>
                </xsl:call-template>,
                <xsl:call-template name="ResolveType">
                    <xsl:with-param name="node" select="/tp:spec/tp:mapping[@name=$type]/tp:member[@name='Value']"/>
                </xsl:call-template>
                &gt;
            </xsl:when>
            <xsl:otherwise>
                <xsl:message terminate="yes">
                    Unspecified type <xsl:value-of select="$type"/>.
                </xsl:message>
            </xsl:otherwise>
        </xsl:choose>
    </xsl:template>
</xsl:stylesheet>
