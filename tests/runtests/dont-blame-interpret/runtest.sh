#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of dont-blame-interpret
#   Description: In case of a crash in interpreter (python, perl, R, tcl, php)
#                abrt should blame the running script and not the interpreter itself.
#   Author: Jiri Moskovcak <jmoskovc@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2011 Red Hat, Inc. All rights reserved.
#
#   This copyrighted material is made available to anyone wishing
#   to use, modify, copy, or redistribute it subject to the terms
#   and conditions of the GNU General Public License version 2.
#
#   This program is distributed in the hope that it will be
#   useful, but WITHOUT ANY WARRANTY; without even the implied
#   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
#   PURPOSE. See the GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public
#   License along with this program; if not, write to the Free
#   Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
#   Boston, MA 02110-1301, USA.
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

. /usr/share/beakerlib/beakerlib.sh
. ../aux/lib.sh

TEST="dont-blame-interpret"
PACKAGE="abrt"

ABRT_SAVE_ACTION="abrt-action-save-package-data -c abrt-action-save-package-data.conf"

INIT_DD_PATH="dump_dirs/ccpp-2020-02-24-11:38:50.153744"

OLD_DUMP_DIR="00001"
PERL_DUMP_DIR="11111"
PHP_DUMP_DIR="22222"
PHP_CGI_DUMP_DIR="33333"
R_DUMP_DIR="44444"
TCL_DUMP_DIR="55555"

function check_assigned_component()
{
    D_D=$INIT_DD_PATH-$1

    rlAssertGrep "will-crash" "$D_D/component"
    rlLog "content of $D_D/component file: \"$(cat $D_D/component)\""
    rlLog "content of $D_D/cmdline file: \"$(cat $D_D/cmdline)\""
}

function prepare_dump_dir()
{
    INTP=$1
    D_D=$2

    cp -ra "$INIT_DD_PATH-$OLD_DUMP_DIR" "$INIT_DD_PATH-$D_D"
    grep -rl '##INTERPRET##' "$INIT_DD_PATH-$D_D" | xargs sed -i "s/##INTERPRET##/$INTP/g"
}

rlJournalStart
    rlPhaseStartSetup

        rlAssertRpm perl-interpreter
        check_prior_crashes
        load_abrt_conf
        TmpDir=$(mktemp -d)

        cp -ra dump_dirs "$TmpDir"
        cp abrt-action-save-package-data.conf "$TmpDir"

        pushd "$TmpDir" || exit
    rlPhaseEnd

    rlPhaseStartTest "Dont blame Python3 interpret"
        generate_python3_segfault
        wait_for_hooks
        get_crash_path

        rlAssertGrep "will-crash" "$crash_PATH/component"
        rlLog "content of $crash_PATH/component file: $(cat $crash_PATH/component)"
        rlLog "content of $crash_PATH/cmdline file: $(cat $crash_PATH/cmdline)"

        remove_problem_directory
    rlPhaseEnd

    rlPhaseStartTest "Dont blame perl interpret"
        PERL_VERSION="$(rpm --queryformat="%{VERSION}" -q perl-interpreter)"
        prepare_dump_dir "perl$PERL_VERSION" "$PERL_DUMP_DIR"

        rlRun "$ABRT_SAVE_ACTION -d $INIT_DD_PATH-$PERL_DUMP_DIR" 0
        check_assigned_component "$PERL_DUMP_DIR"
    rlPhaseEnd

    rlPhaseStartTest "Dont blame php interpret"
        prepare_dump_dir "php" "$PHP_DUMP_DIR"

        rlRun "$ABRT_SAVE_ACTION -d $INIT_DD_PATH-$PHP_DUMP_DIR" 0
        check_assigned_component "$PHP_DUMP_DIR"
    rlPhaseEnd

    rlPhaseStartTest "Dont blame php-cgi interpret"
        prepare_dump_dir "php-cgi" "$PHP_CGI_DUMP_DIR"

        rlRun "$ABRT_SAVE_ACTION -d $INIT_DD_PATH-$PHP_CGI_DUMP_DIR" 0
        check_assigned_component "$PHP_CGI_DUMP_DIR"
    rlPhaseEnd

    rlPhaseStartTest "Dont blame R interpret"
        prepare_dump_dir "R" "$R_DUMP_DIR"

        rlRun "$ABRT_SAVE_ACTION -d $INIT_DD_PATH-$R_DUMP_DIR" 0
        check_assigned_component "$R_DUMP_DIR"
    rlPhaseEnd

    rlPhaseStartTest "Dont blame tcl interpret"
        prepare_dump_dir "tclsh" "$TCL_DUMP_DIR"

        rlRun "$ABRT_SAVE_ACTION -d $INIT_DD_PATH-$TCL_DUMP_DIR" 0
        check_assigned_component "$TCL_DUMP_DIR"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "popd"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
