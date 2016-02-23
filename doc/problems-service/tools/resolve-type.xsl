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

    <xsl:template name="DBusTypeSuffix">
        <xsl:param name="type"/>
        <xsl:param name="closure"/>

        <xsl:if test="starts-with($closure, '.')">
        &gt;
            <xsl:call-template name="DBusType">
                <xsl:with-param name="type" select="substring($type, 2)"/>
                <xsl:with-param name="closure" select="substring($closure, 2)"/>
            </xsl:call-template>
        </xsl:if>
        <xsl:if test="not(starts-with($closure, '.'))">
            <xsl:call-template name="DBusType">
                <xsl:with-param name="type" select="substring($type, 2)"/>
                <xsl:with-param name="closure" select="$closure"/>
            </xsl:call-template>
        </xsl:if>
    </xsl:template>

    <!-- Map a D-Bus type to its EggDBus counterpart -->
    <xsl:template name="DBusType">
        <xsl:param name="type"/>
        <xsl:param name="closure"/>
        <xsl:param name="start"/>

        <xsl:if test="$type!=''">
            <xsl:choose>
                <xsl:when test="starts-with($type, 'o')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    ObjectPath
                    <xsl:call-template name="DBusTypeSuffix">
                        <xsl:with-param name="type" select="$type"/>
                        <xsl:with-param name="closure" select="$closure"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, 's')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    String
                    <xsl:call-template name="DBusTypeSuffix">
                        <xsl:with-param name="type" select="$type"/>
                        <xsl:with-param name="closure" select="$closure"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, 'y')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    Byte
                    <xsl:call-template name="DBusTypeSuffix">
                        <xsl:with-param name="type" select="$type"/>
                        <xsl:with-param name="closure" select="$closure"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, 'b')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    Boolean
                    <xsl:call-template name="DBusTypeSuffix">
                        <xsl:with-param name="type" select="$type"/>
                        <xsl:with-param name="closure" select="$closure"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, 'n')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    Int16
                    <xsl:call-template name="DBusTypeSuffix">
                        <xsl:with-param name="type" select="$type"/>
                        <xsl:with-param name="closure" select="$closure"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, 'q')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    UInt16
                    <xsl:call-template name="DBusTypeSuffix">
                        <xsl:with-param name="type" select="$type"/>
                        <xsl:with-param name="closure" select="$closure"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, 'i')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    Int32
                    <xsl:call-template name="DBusTypeSuffix">
                        <xsl:with-param name="type" select="$type"/>
                        <xsl:with-param name="closure" select="$closure"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, 'u')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    UInt32
                    <xsl:call-template name="DBusTypeSuffix">
                        <xsl:with-param name="type" select="$type"/>
                        <xsl:with-param name="closure" select="$closure"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, 'x')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    Int64
                    <xsl:call-template name="DBusTypeSuffix">
                        <xsl:with-param name="type" select="$type"/>
                        <xsl:with-param name="closure" select="$closure"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, 't')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    UInt64
                    <xsl:call-template name="DBusTypeSuffix">
                        <xsl:with-param name="type" select="$type"/>
                        <xsl:with-param name="closure" select="$closure"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, 'd')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    Double
                    <xsl:call-template name="DBusTypeSuffix">
                        <xsl:with-param name="type" select="$type"/>
                        <xsl:with-param name="closure" select="$closure"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, 'g')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    Signature
                    <xsl:call-template name="DBusTypeSuffix">
                        <xsl:with-param name="type" select="$type"/>
                        <xsl:with-param name="closure" select="$closure"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, 'v')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    Variant
                    <xsl:call-template name="DBusTypeSuffix">
                        <xsl:with-param name="type" select="$type"/>
                        <xsl:with-param name="closure" select="$closure"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, 'a{')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    Dict&lt;
                    <xsl:call-template name="DBusType">
                        <xsl:with-param name="type" select="substring($type, 3)"/>
                        <xsl:with-param name="closure">}<xsl:value-of select="$closure"/></xsl:with-param>
                        <xsl:with-param name="start">1</xsl:with-param>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, 'a')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    Array&lt;
                    <xsl:call-template name="DBusType">
                        <xsl:with-param name="type" select="substring($type, 2)"/>
                        <xsl:with-param name="closure">.<xsl:value-of select="$closure"/></xsl:with-param>
                        <xsl:with-param name="start">1</xsl:with-param>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, '(')">
                    <xsl:if test="$closure!='' and $start=''">,</xsl:if>
                    Struct&lt;
                    <xsl:call-template name="DBusType">
                        <xsl:with-param name="type" select="substring($type, 2)"/>
                        <xsl:with-param name="closure">)<xsl:value-of select="$closure"/></xsl:with-param>
                        <xsl:with-param name="start">1</xsl:with-param>
                    </xsl:call-template>
                </xsl:when>
                <xsl:when test="starts-with($type, substring($closure, 1, 1))">
                    &gt;
                    <xsl:call-template name="DBusTypeSuffix">
                        <xsl:with-param name="type" select="$type"/>
                        <xsl:with-param name="closure" select="substring($closure, 2)"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:if test="$closure=''">
                        <xsl:message terminate="yes">
                            Unknown DBus Type <xsl:value-of select="$type"/>
                        </xsl:message>
                    </xsl:if>
                    <xsl:message terminate="yes">
                        Expected <xsl:value-of select="substring($closure, 1, 1)"/>, got <xsl:value-of select="substring($type, 1, 1)"/>
                    </xsl:message>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:if>
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
