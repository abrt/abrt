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

abrt_conf="/etc/abrt/abrt.conf"

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
              string:"com.redhat.problems.configuration.$1" string:$2 | sed 1d | tr -d "\n" | tr -s " " | cut -f 4- -d ' '
}

rlJournalStart
    rlPhaseStartSetup
        rlWaitForCmd "killall -qw abrt-configuration" -t 5

        INTERFACES_DIR=`pkg-config --variable=problemsconfigurationdir abrt`
        export INTERFACES_DIR

        rlRun "DEFAULT_AUTO_REPORTING=\"$(augtool get /files/usr/share/abrt/conf.d/abrt.conf/AutoreportingEnabled | cut -d' ' -f3)\"" 0
        rlRun "ABRT_AUTO_REPORTING=\"$(augtool get /files/etc/abrt/abrt.conf/AutoreportingEnabled | cut -d' ' -f3)\"" 0
        rlRun "augtool set /files/etc/abrt/abrt.conf/AutoreportingEnabled $DEFAULT_AUTO_REPORTING" 0

        if [ "xyes" == "x$DEFAULT_AUTO_REPORTING" ]; then
            DEFAULT_AUTO_REPORTING="true"
            SETTO_AUTO_REPORTING="False"
            MODIFIED_AUTO_REPORTING="false"
        else
            DEFAULT_AUTO_REPORTING="false"
            SETTO_AUTO_REPORTING="True"
            MODIFIED_AUTO_REPORTING="true"
        fi
    rlPhaseEnd

    rlPhaseStartTest "Invalid XML interface file"
        LOG_FILE="./invalid_xml.log"
        INVALID_XML_PATH=$INTERFACES_DIR/com.redhat.problems.configuration.invalid.xml

        echo "<node><interface></foo></blah>" > $INVALID_XML_PATH

        rlRun "abrt-configuration >$LOG_FILE 2>&1 &"
        rlWaitForCmd "killall -w abrt-configuration" -t 5

        rlAssertGrep "Could not parse interface file '$INVALID_XML_PATH':" $LOG_FILE

        rlRun "rm $INVALID_XML_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Missing node's both conf files"
        LOG_FILE="./missing_both_node_configuration_annots.log"
        XML_PATH=$INTERFACES_DIR/com.redhat.problems.configuration.missing_both_node_configuration_annots.xml

        echo "<node name='/foo/blah'><interface name='foo.blah'><property name='success' type='b' access='readwrite'/></interface></node>" > $XML_PATH

        rlRun "abrt-configuration >$LOG_FILE 2>&1 &"
        rlWaitForCmd "killall -w abrt-configuration" -t 5

        rlAssertGrep "Configuration node '/foo/blah' misses annotation 'com.redhat.problems.ConfFile'" $LOG_FILE
        rlAssertGrep "Configuration node '/foo/blah' misses annotation 'com.redhat.problems.DefaultConfFile'" $LOG_FILE

        rlRun "rm $XML_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Missing node's working conf file annotation"
        LOG_FILE="./missing_working_node_configuration_annot.log"
        XML_PATH=$INTERFACES_DIR/com.redhat.problems.configuration.missing_working_node_configuration_annot.xml

        echo "<node name='/foo/blah'><annotation name='com.redhat.problems.DefaultConfFile' value='/etc/abrt/abrt.conf'/><interface name='foo.blah'><property name='success' type='b' access='readwrite'/></interface></node>" > $XML_PATH

        rlRun "abrt-configuration >$LOG_FILE 2>&1 &"
        rlWaitForCmd "killall -w abrt-configuration" -t 5

        rlAssertGrep "Configuration node '/foo/blah' misses annotation 'com.redhat.problems.ConfFile'" $LOG_FILE

        rlRun "rm $XML_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Missing node's default conf file annotation"
        LOG_FILE="./missing_default_node_configuration_annot.log"
        XML_PATH=$INTERFACES_DIR/com.redhat.problems.configuration.missing_default_node_configuration_annot.xml

        echo "<node name='/foo/blah'><annotation name='com.redhat.problems.ConfFile' value='/etc/abrt/abrt.conf'/><interface name='foo.blah'><property name='success' type='b' access='readwrite'/></interface></node>" > $XML_PATH

        rlRun "abrt-configuration >$LOG_FILE 2>&1 &"
        rlWaitForCmd "killall -w abrt-configuration" -t 5

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
        rlWaitForCmd "killall -w abrt-configuration" -t 5

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
        rlWaitForCmd "killall -w abrt-configuration" -t 5

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

        rlWaitForCmd "killall -w abrt-configuration" -t 5

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

        rlAssertEquals "Get 'AutoreportingEnabled' value" "_$(confDBusGetProperty blah AutoreportingEnabled)" "_$DEFAULT_AUTO_REPORTING"
        rlRun "confDBusSetProperty blah AutoreportingEnabled boolean $SETTO_AUTO_REPORTING" 0

        rlAssertExists "/tmp/abrt.conf"

        rlAssertEquals "Get 'AutoreportingEnabled' value" "_$(confDBusGetProperty blah AutoreportingEnabled)" "_$MODIFIED_AUTO_REPORTING"
        rlRun "confDBusSetPropertyDefault blah AutoreportingEnabled" 0
        rlAssertEquals "Reset 'AutoreportingEnabled' value" "_$(confDBusGetProperty blah AutoreportingEnabled)" "_$DEFAULT_AUTO_REPORTING"

        rlWaitForCmd "killall -w abrt-configuration" -t 5

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

        rlWaitForCmd "killall -w abrt-configuration" -t 5

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

        rlWaitForCmd "killall -w abrt-configuration" -t 5

        rlAssertGrep "Property 'SuperSuccess' has unsupported getter type" $LOG_FILE
        rlAssertGrep "Property 'SuperSuccess' has unsupported setter type" $LOG_FILE
        rlAssertGrep "Error .*abrt.*reflection.*unsupported.*type.*error.*129: Type with signature 'ab' is not supported" $RUN_LOG_FILE

        rlRun "rm $XML_PATH"
    rlPhaseEnd

    rlPhaseStartTest "Boolean"
        rlAssertEquals "Get 'AutoreportingEnabled' value" "_$(confDBusGetProperty abrt AutoreportingEnabled)" "_$DEFAULT_AUTO_REPORTING"
        rlRun "confDBusSetProperty abrt AutoreportingEnabled boolean $SETTO_AUTO_REPORTING" 0
        rlAssertEquals "Set 'AutoreportingEnabled' value" "_$(confDBusGetProperty abrt AutoreportingEnabled)" "_$MODIFIED_AUTO_REPORTING"
        rlRun "confDBusSetPropertyDefault abrt AutoreportingEnabled" 0
        rlAssertEquals "Reset 'AutoreportingEnabled' value" "_$(confDBusGetProperty abrt AutoreportingEnabled)" "_$DEFAULT_AUTO_REPORTING"
    rlPhaseEnd

    rlPhaseStartTest "String"
        rlAssertEquals "Get 'AutoreportingEvent' value" "_$(confDBusGetProperty abrt AutoreportingEvent)" "_\"report_uReport\""
        rlRun "confDBusSetProperty abrt AutoreportingEvent string '\"report_Bugzilla\"'" 0
        rlAssertEquals "Failed to set 'AutoreportingEvent' value" "_$(confDBusGetProperty abrt AutoreportingEvent)" "_\"report_Bugzilla\""
        rlRun "confDBusSetPropertyDefault abrt AutoreportingEvent" 0
        rlAssertEquals "Failed to reset 'AutoreportingEvent' value" "_$(confDBusGetProperty abrt AutoreportingEvent)" "_\"report_uReport\""
    rlPhaseEnd

    rlPhaseStartTest "Int32"
        rlFileBackup $abrt_conf
        # change config (max size was reduced in aux/pre.sh due to limited available space on VM)
        augtool set /files/etc/abrt/abrt.conf/MaxCrashReportsSize 5000

        rlAssertEquals "Get 'MaxCrashReportsSize' value" "_$(confDBusGetProperty abrt MaxCrashReportsSize)" "_5000"
        rlRun "confDBusSetProperty abrt MaxCrashReportsSize int32 1234" 0
        rlAssertEquals "Failed to set 'MaxCrashReportsSize' value" "_$(confDBusGetProperty abrt MaxCrashReportsSize)" "_1234"
        rlRun "confDBusSetPropertyDefault abrt MaxCrashReportsSize" 0
        rlAssertEquals "Failed to reset 'MaxCrashReportsSize' value" "_$(confDBusGetProperty abrt MaxCrashReportsSize)" "_5000"

        rlFileRestore
    rlPhaseEnd

    rlPhaseStartTest "String Array from non-default file"

        # get option Interpreters from 'etc/abrt/abrt-action-save-package-data.conf'
        rlRun "interpreters_conf=`augtool get /files/etc/abrt/abrt-action-save-package-data.conf/Interpreters | cut -d'=' -f2 | tr -d ' '`"

        # get option Interpreters by DBus
        rlRun "confDBusGetProperty abrt Interpreters" 0
        interpreters_dbus=$(confDBusGetProperty abrt Interpreters)
        rlRun "interpreters_dbus=`echo $interpreters_dbus | sed 's/string \"//g' | sed 's/\" ]//g' | tr -d '[] ' | tr '"' ','`"
        rlAssertEquals "Get 'Interpreters' value" "_$interpreters_dbus" "_$interpreters_conf"

        rlRun "confDBusSetProperty abrt Interpreters array:string '[\"foo\",\"blah\",\"panda\"]'" 0
        rlAssertEquals "Failed to set 'Interpreters ' value" "_$(confDBusGetProperty abrt Interpreters)" \
            '_[ string "foo" string "blah" string "panda" ]'
        rlRun "confDBusSetPropertyDefault abrt Interpreters" 0

        # get option Interpreters from 'etc/abrt/abrt-action-save-package-data.conf'
        rlRun "interpreters_conf=`augtool get /files/etc/abrt/abrt-action-save-package-data.conf/Interpreters | cut -d'=' -f2 | tr -d ' '`"

        # get option Interpreters by DBus
        rlRun "confDBusGetProperty abrt Interpreters" 0
        interpreters_dbus=$(confDBusGetProperty abrt Interpreters)
        rlRun "interpreters_dbus=`echo $interpreters_dbus | sed 's/string \"//g' | sed 's/\" ]//g' | tr -d '[] ' | tr '"' ','`"
        rlAssertEquals "Get 'Interpreters' value" "_$interpreters_dbus" "_$interpreters_conf"
    rlPhaseEnd

    rlPhaseStartTest "Boolean from non-default file"
        # get option 'ProcessUnpackaged' from conf file and translate "no" -> "false" and "yes" -> "true"
        rlRun "process_unpackaged_conf=`augtool get /files/etc/abrt/abrt-action-save-package-data.conf/ProcessUnpackaged | cut -d'=' -f2 | tr -d ' ' | sed s/no/false/g | sed s/yes/true/g`"
        rlAssertEquals "Get 'ProcessUnpackaged' value" "_$(confDBusGetProperty abrt ProcessUnpackaged)" "_$process_unpackaged_conf"

        rlRun "confDBusSetProperty abrt ProcessUnpackaged boolean False" 0
        rlAssertEquals "Set 'ProcessUnpackaged' value" "_$(confDBusGetProperty abrt ProcessUnpackaged)" "_false"
        rlRun "confDBusSetProperty abrt ProcessUnpackaged boolean True" 0
        rlAssertEquals "Set 'ProcessUnpackaged' value" "_$(confDBusGetProperty abrt ProcessUnpackaged)" "_true"

        rlRun "confDBusSetPropertyDefault abrt ProcessUnpackaged" 0
        # get option 'ProcessUnpackaged' from conf file and translate "no" -> "false" and "yes" -> "true"
        rlRun "process_unpackaged_conf=`augtool get /files/etc/abrt/abrt-action-save-package-data.conf/ProcessUnpackaged | cut -d'=' -f2 | tr -d ' ' | sed s/no/false/g | sed s/yes/true/g`"
        rlAssertEquals "Reset 'ProcessUnpackaged' value" "_$(confDBusGetProperty abrt ProcessUnpackaged)" "_$process_unpackaged_conf"
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
        rlRun "augtool set /files/etc/abrt/abrt.conf/AutoreportingEnabled $ABRT_AUTO_REPORTING" 0 "Restore AutoreportingEnabled"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
