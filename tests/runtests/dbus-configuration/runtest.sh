#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of dbus-configuration
#   Description: Check functions for changing configuration
#   Author: Jakub Filak <jfilak@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2013 Red Hat, Inc. All rights reserved.
#
#   This program is free software: you can redistribute it and/or
#   modify it under the terms of the GNU General Public License as
#   published by the Free Software Foundation, either version 3 of
#   the License, or (at your option) any later version.
#
#   This program is distributed in the hope that it will be
#   useful, but WITHOUT ANY WARRANTY; without even the implied
#   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
#   PURPOSE.  See the GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program. If not, see http://www.gnu.org/licenses/.
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

. /usr/share/beakerlib/beakerlib.sh
. ../aux/lib.sh

TEST="dbus-configuration"
PACKAGE="abrt"

function confDBusSetProperty() {
    # dbus-send does not support variant:array:string
    #
    # dbus-send --system --type=method_call --print-reply \
    #          --dest=com.redhat.problems.configuration /com/redhat/problems/configuration/$1 org.freedesktop.DBus.Properties.Set \
    #          string:"com.redhat.problems.configuration.$1" string:$2 variant:$3:$4 | awk -v first=1 '{ if (first) { first=0 } else { print } } /Error/ { print }'
    #
    echo "import dbus; bus = dbus.SystemBus(); proxy = bus.get_object(\"com.redhat.problems.configuration\", \"/com/redhat/problems/configuration/$1\"); problems = dbus.Interface(proxy, dbus_interface=\"org.freedesktop.DBus.Properties\"); problems.Set(\"com.redhat.problems.configuration.$1\", \"$2\", $4)" | python
}

function confDBusSetPropertyDefault() {
    dbus-send --system --type=method_call --print-reply \
              --dest=com.redhat.problems.configuration /com/redhat/problems/configuration/$1 com.redhat.problems.configuration.SetDefault \
              string:$2 | awk -v first=1 '{ if (first) { first=0 } else { print} } /Error/ { print }'
}

function confDBusGetProperty() {
    dbus-send --system --type=method_call --print-reply \
              --dest=com.redhat.problems.configuration /com/redhat/problems/configuration/$1 org.freedesktop.DBus.Properties.Get \
              string:"com.redhat.problems.configuration.$1" string:$2 | sed 1d | tr "\n" " " | tr -s " " | cut -f 4- -d ' '
}

rlJournalStart
    rlPhaseStartSetup
        killall abrt-configuration

        INTERFACES_DIR=`pkg-config --variable=problemsconfigurationdir abrt`
        export INTERFACES_DIR
    rlPhaseEnd

    rlPhaseStartTest "Invalid XML interface file"
        LOG_FILE="./invalid_xml.log"
        INVALID_XML_PATH=$INTERFACES_DIR/com.redhat.problems.configuration.invalid.xml

        echo "<node><interface></foo></blah>" > $INVALID_XML_PATH

        rlRun "abrt-configuration >$LOG_FILE 2>&1 &"
        killall abrt-configuration

        rlAssertGrep "Could not parse interface file '$INVALID_XML_PATH':" $LOG_FILE

        rlRun "rm $INVALID_XML_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Missing node's both conf files"
        LOG_FILE="./missing_both_node_configuration_annots.log"
        XML_PATH=$INTERFACES_DIR/com.redhat.problems.configuration.missing_both_node_configuration_annots.xml

        echo "<node name='/foo/blah'><interface name='foo.blah'><property name='success' type='b' access='readwrite'/></interface></node>" > $XML_PATH

        rlRun "abrt-configuration >$LOG_FILE 2>&1 &"
        killall abrt-configuration

        rlAssertGrep "Configuration node '/foo/blah' misses annotation 'com.redhat.problems.ConfFile'" $LOG_FILE
        rlAssertGrep "Configuration node '/foo/blah' misses annotation 'com.redhat.problems.DefaultConfFile'" $LOG_FILE

        rlRun "rm $XML_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Missing node's working conf file annotation"
        LOG_FILE="./missing_working_node_configuration_annot.log"
        XML_PATH=$INTERFACES_DIR/com.redhat.problems.configuration.missing_working_node_configuration_annot.xml

        echo "<node name='/foo/blah'><annotation name='com.redhat.problems.DefaultConfFile' value='/etc/abrt/abrt.conf'/><interface name='foo.blah'><property name='success' type='b' access='readwrite'/></interface></node>" > $XML_PATH

        rlRun "abrt-configuration >$LOG_FILE 2>&1 &"
        killall abrt-configuration

        rlAssertGrep "Configuration node '/foo/blah' misses annotation 'com.redhat.problems.ConfFile'" $LOG_FILE

        rlRun "rm $XML_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Missing node's default conf file annotation"
        LOG_FILE="./missing_default_node_configuration_annot.log"
        XML_PATH=$INTERFACES_DIR/com.redhat.problems.configuration.missing_default_node_configuration_annot.xml

        echo "<node name='/foo/blah'><annotation name='com.redhat.problems.ConfFile' value='/etc/abrt/abrt.conf'/><interface name='foo.blah'><property name='success' type='b' access='readwrite'/></interface></node>" > $XML_PATH

        rlRun "abrt-configuration >$LOG_FILE 2>&1 &"
        killall abrt-configuration

        rlAssertGrep "Configuration node '/foo/blah' misses annotation 'com.redhat.problems.DefaultConfFile'" $LOG_FILE

        rlRun "rm $XML_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Only property's working annotation"
        LOG_FILE="./only_working_property_configuration_annot.log"
        XML_PATH=$INTERFACES_DIR/com.redhat.problems.configuration.only_working_property_configuration_annot.xml

        echo "<node name='/foo/blah'>" \
             "<annotation name='com.redhat.problems.ConfFile' value='/etc/abrt/abrt.conf'/>" \
             "<annotation name='com.redhat.problems.DefaultConfFile' value='/etc/abrt/abrt.conf'/>" \
             "<interface name='foo.blah'>" \
             "<property name='success' type='b' access='readwrite'>" \
             "<annotation name='com.redhat.problems.ConfFile' value='/etc/abrt/abrt.conf'/>" \
             "</property>" \
             "</interface></node>" \
             > $XML_PATH

        rlRun "abrt-configuration >$LOG_FILE 2>&1 &"
        killall abrt-configuration

        rlAssertGrep "Property 'success' misses annotation 'com.redhat.problems.DefaultConfFile'" $LOG_FILE

        rlRun "rm $XML_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Only property's default annotation"
        LOG_FILE="./only_default_property_configuration_annot.log"
        XML_PATH=$INTERFACES_DIR/com.redhat.problems.configuration.only_default_property_configuration_annot.xml

        echo "<node name='/foo/blah'>" \
             "<annotation name='com.redhat.problems.ConfFile' value='/etc/abrt/abrt.conf'/>" \
             "<annotation name='com.redhat.problems.DefaultConfFile' value='/etc/abrt/abrt.conf'/>" \
             "<interface name='foo.blah'>" \
             "<property name='success' type='b' access='readwrite'>" \
             "<annotation name='com.redhat.problems.DefaultConfFile' value='/etc/abrt/abrt.conf'/>" \
             "</property>" \
             "</interface></node>" \
             > $XML_PATH

        rlRun "abrt-configuration >$LOG_FILE 2>&1 &"
        killall abrt-configuration

        rlAssertGrep "Property 'success' misses annotation 'com.redhat.problems.ConfFile'" $LOG_FILE

        rlRun "rm $XML_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Not existing node's configuration files"
        LOG_FILE="./not_existing_node_configuration.log"
        RUN_LOG_FILE="./not_existing_node_configuration_run.log"
        XML_PATH=$INTERFACES_DIR/com.redhat.problems.configuration.not_existing_node_configuration.xml

        echo "<node name='/com/redhat/problems/configuration/blah'>" \
             "<annotation name='com.redhat.problems.ConfFile' value='/proc/cpuinfo/foo/abrt.conf'/>" \
             "<annotation name='com.redhat.problems.DefaultConfFile' value='/proc/cpuinfo/foo/blah/abrt.conf'/>" \
             "<interface name='com.redhat.problems.configuration.blah'>" \
             "<property name='AutoreportingEnabled' type='b' access='readwrite'/>" \
             "</interface></node>" \
             > $XML_PATH

        rlRun "abrt-configuration >$LOG_FILE 2>&1 &"

        rlRun "confDBusGetProperty blah AutoreportingEnabled >$RUN_LOG_FILE 2>&1" 0
        rlRun "confDBusSetProperty blah AutoreportingEnabled boolean True" 1

        rlAssertGrep "Error .*abrt.*file.*access.*error.*: Could not load configuration from '/proc/cpuinfo/foo/blah/abrt.conf'" $RUN_LOG_FILE

        killall abrt-configuration

        rlRun "rm $XML_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Not existing node's working file"
        LOG_FILE="./not_existing_node_configuration.log"
        XML_PATH=$INTERFACES_DIR/com.redhat.problems.configuration.not_existing_node_configuration.xml

        rlRun "rm -f /tmp/abrt.conf"

        echo "<node name='/com/redhat/problems/configuration/blah'>" \
             "<annotation name='com.redhat.problems.ConfFile' value='/tmp/abrt.conf'/>" \
             "<annotation name='com.redhat.problems.DefaultConfFile' value='/usr/share/abrt/conf.d/abrt.conf'/>" \
             "<interface name='com.redhat.problems.configuration.blah'>" \
             "<property name='AutoreportingEnabled' type='b' access='readwrite'/>" \
             "</interface></node>" \
             > $XML_PATH

        rlRun "abrt-configuration >$LOG_FILE 2>&1 &"

        rlAssertEquals "Get 'AutoreportingEnabled' value" "_$(confDBusGetProperty blah AutoreportingEnabled)" "_false"
        rlRun "confDBusSetProperty blah AutoreportingEnabled boolean True" 0

        rlAssertExists "/tmp/abrt.conf"

        rlAssertEquals "Get 'AutoreportingEnabled' value" "_$(confDBusGetProperty blah AutoreportingEnabled)" "_true"

        killall abrt-configuration

        rlRun "rm $XML_PATH"
        rlRun "rm /tmp/abrt.conf"
    rlPhaseEnd

    rlPhaseStartTest "Not existing node's default file"
        LOG_FILE="./not_existing_default_node_configuration.log"
        RUN_LOG_FILE="./not_existing_default_node_configuration_run.log"
        XML_PATH=$INTERFACES_DIR/com.redhat.problems.configuration.not_existing_default_node_configuration.xml

        rlRun "rm -f /tmp/abrt.conf"

        echo "<node name='/com/redhat/problems/configuration/blah'>" \
             "<annotation name='com.redhat.problems.ConfFile' value='/tmp/abrt.conf'/>" \
             "<annotation name='com.redhat.problems.DefaultConfFile' value='/proc/cpuinfo/abrt/conf.d/abrt.conf'/>" \
             "<interface name='com.redhat.problems.configuration.blah'>" \
             "<property name='AutoreportingEnabled' type='b' access='readwrite'/>" \
             "</interface></node>" \
             > $XML_PATH

        rlRun "abrt-configuration >$LOG_FILE 2>&1 &"

        rlRun "confDBusGetProperty blah AutoreportingEnabled >$RUN_LOG_FILE 2>&1" 0
        rlRun "confDBusSetProperty blah AutoreportingEnabled boolean False" 0

        rlAssertExists "/tmp/abrt.conf"

        rlRun "confDBusGetProperty blah AutoreportingEnabled >>$RUN_LOG_FILE 2>&1" 0

        rlAssertGrep "Error .*abrt.*file.*access.*error.*: Could not load configuration from '/proc/cpuinfo/abrt/conf.d/abrt.conf'" $RUN_LOG_FILE

        killall abrt-configuration

        rlRun "rm $XML_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Unsupported property type"
        LOG_FILE="./unsupported_type.log"
        RUN_LOG_FILE="./unsupported_type_run.log"
        XML_PATH=$INTERFACES_DIR/com.redhat.problems.configuration.unsupported_type.xml

        echo "<node name='/com/redhat/problems/configuration/blah'>" \
             "<annotation name='com.redhat.problems.ConfFile' value='/tmp/abrt.conf'/>" \
             "<annotation name='com.redhat.problems.DefaultConfFile' value='/usr/share/abrt/conf.d/abrt.conf'/>" \
             "<interface name='com.redhat.problems.configuration.blah'>" \
             "<property name='SuperSuccess' type='ab' access='readwrite'/>" \
             "</interface></node>" \
             > $XML_PATH

        rlRun "abrt-configuration >$LOG_FILE 2>&1 &"

        rlRun "confDBusGetProperty blah SuperSuccess >$RUN_LOG_FILE 2>&1"
        rlRun "confDBusSetProperty blah SuperSuccess array:boolean '[False]'" 1

        killall abrt-configuration

        rlAssertGrep "Property 'SuperSuccess' has unsupported getter type" $LOG_FILE
        rlAssertGrep "Property 'SuperSuccess' has unsupported setter type" $LOG_FILE
        rlAssertGrep "Error .*abrt.*reflection.*unsupported.*type.*error.*129: Type with signature 'ab' is not supported" $RUN_LOG_FILE

        rlRun "rm $XML_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Boolean"
        rlAssertEquals "Get 'AutoreportingEnabled' value" "_$(confDBusGetProperty abrt AutoreportingEnabled)" "_false"
        rlRun "confDBusSetProperty abrt AutoreportingEnabled boolean True" 0
        rlAssertEquals "Set 'AutoreportingEnabled' value" "_$(confDBusGetProperty abrt AutoreportingEnabled)" "_true"
        rlRun "confDBusSetPropertyDefault abrt AutoreportingEnabled" 0
        rlAssertEquals "Reset 'AutoreportingEnabled' value" "_$(confDBusGetProperty abrt AutoreportingEnabled)" "_false"
    rlPhaseEnd

    rlPhaseStartTest "String"
        rlAssertEquals "Get 'AutoreportingEvent' value" "_$(confDBusGetProperty abrt AutoreportingEvent)" "_\"report_uReport\""
        rlRun "confDBusSetProperty abrt AutoreportingEvent string '\"report_Bugzilla\"'" 0
        rlAssertEquals "Failed to set 'AutoreportingEvent' value" "_$(confDBusGetProperty abrt AutoreportingEvent)" "_\"report_Bugzilla\""
        rlRun "confDBusSetPropertyDefault abrt AutoreportingEvent" 0
        rlAssertEquals "Failed to reset 'AutoreportingEvent' value" "_$(confDBusGetProperty abrt AutoreportingEvent)" "_\"report_uReport\""
    rlPhaseEnd

    rlPhaseStartTest "Int32"
        rlAssertEquals "Get 'MaxCrashReportsSize' value" "_$(confDBusGetProperty abrt MaxCrashReportsSize)" "_1000"
        rlRun "confDBusSetProperty abrt MaxCrashReportsSize int32 1234" 0
        rlAssertEquals "Failed to set 'MaxCrashReportsSize' value" "_$(confDBusGetProperty abrt MaxCrashReportsSize)" "_1234"
        rlRun "confDBusSetPropertyDefault abrt MaxCrashReportsSize" 0
        rlAssertEquals "Failed to reset 'MaxCrashReportsSize' value" "_$(confDBusGetProperty abrt MaxCrashReportsSize)" "_1000"
    rlPhaseEnd

    rlPhaseStartTest "String Array from non-default file"
        rlRun "confDBusGetProperty abrt Interpreters" 0
        rlAssertEquals "Get 'Interpreters' value" "_$(confDBusGetProperty abrt Interpreters)" \
            '_[ string "python2" string "python2.7" string "python" string "python3" string "python3.3" string "perl" string "perl5.16.2" ]'
        rlRun "confDBusSetProperty abrt Interpreters array:string '[\"foo\",\"blah\",\"panda\"]'" 0
        rlAssertEquals "Failed to set 'Interpreters ' value" "_$(confDBusGetProperty abrt Interpreters)" \
            '_[ string "foo" string "blah" string "panda" ]'
        rlRun "confDBusSetPropertyDefault abrt Interpreters" 0
        rlAssertEquals "Get 'Interpreters' value" "_$(confDBusGetProperty abrt Interpreters)" \
            '_[ string "python2" string "python2.7" string "python" string "python3" string "python3.3" string "perl" string "perl5.16.2" ]'
    rlPhaseEnd

    rlPhaseStartTest "Boolean from non-default file"
        rlAssertEquals "Get 'OpenGPGCheck' value" "_$(confDBusGetProperty abrt OpenGPGCheck)" "_true"
        rlRun "confDBusSetProperty abrt OpenGPGCheck boolean False" 0
        rlAssertEquals "Set 'OpenGPGCheck' value" "_$(confDBusGetProperty abrt OpenGPGCheck)" "_false"
        rlRun "confDBusSetPropertyDefault abrt OpenGPGCheck" 0
        rlAssertEquals "Reset 'OpenGPGCheck' value" "_$(confDBusGetProperty abrt OpenGPGCheck)" "_true"
    rlPhaseEnd

    rlPhaseStartTest "Empty Int32 Value from non-default file"
        rlAssertEquals "Get 'VerboseLog' value" "_$(confDBusGetProperty ccpp VerboseLog)" "_"
        rlRun "confDBusSetProperty ccpp VerboseLog int32 3" 0
        rlAssertEquals "Set 'VerboseLog' value" "_$(confDBusGetProperty ccpp VerboseLog)" "_3"
        rlRun "confDBusSetPropertyDefault ccpp VerboseLog" 0
        rlAssertEquals "Reset 'VerboseLog' value" "_$(confDBusGetProperty ccpp VerboseLog)" "_"
    rlPhaseEnd

    rlPhaseStartTest "Read/Write Python configuration"
        rlAssertEquals "Get 'RequireAbsolutePath' value" "_$(confDBusGetProperty python RequireAbsolutePath)" "_"
        rlRun "confDBusSetProperty python RequireAbsolutePath boolean False" 0
        rlAssertEquals "Set 'RequireAbsolutePath' value" "_$(confDBusGetProperty python RequireAbsolutePath)" "_false"
        rlRun "confDBusSetPropertyDefault python RequireAbsolutePath" 0
        rlAssertEquals "Reset 'RequireAbsolutePath' value" "_$(confDBusGetProperty python RequireAbsolutePath)" "_"
    rlPhaseEnd

    rlPhaseStartTest "Read/Write VMcore configuration"
        rlAssertEquals "Get 'CopyVMcore' value" "_$(confDBusGetProperty vmcore CopyVMcore)" "_true"
        rlRun "confDBusSetProperty vmcore CopyVMcore boolean False" 0
        rlAssertEquals "Set 'CopyVMcore' value" "_$(confDBusGetProperty vmcore CopyVMcore)" "_false"
        rlRun "confDBusSetPropertyDefault vmcore CopyVMcore" 0
        rlAssertEquals "Reset 'CopyVMcore' value" "_$(confDBusGetProperty vmcore CopyVMcore)" "_true"
    rlPhaseEnd

    rlPhaseStartTest "Read/Write XOrg configuration"
        rlAssertEquals "Get 'BlacklistedXorgModules' value" "_$(confDBusGetProperty xorg BlacklistedXorgModules)" \
            '_[ string "nvidia" string "fglrx" string "vboxvideo" ]'
        rlRun "confDBusSetProperty xorg BlacklistedXorgModules array:string '[\"foo\",\"blah\",\"panda\"]'" 0
        rlAssertEquals "Get 'BlacklistedXorgModules' value" "_$(confDBusGetProperty xorg BlacklistedXorgModules)" \
            '_[ string "foo" string "blah" string "panda" ]'
        rlRun "confDBusSetPropertyDefault xorg BlacklistedXorgModules" 0
        rlAssertEquals "Get 'BlacklistedXorgModules' value" "_$(confDBusGetProperty xorg BlacklistedXorgModules)" \
            '_[ string "nvidia" string "fglrx" string "vboxvideo" ]'
    rlPhaseEnd

    rlPhaseStartCleanup
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
