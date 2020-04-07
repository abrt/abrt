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

DUMP_DIRECTORY="dump_dirs/ccpp-2020-02-24-11:38:50.153744"

OLD_DUMP_DIR="00001"
PERL_DUMP_DIR="11111"
PHP_DUMP_DIR="22222"
PHP_CGI_DUMP_DIR="33333"
R_DUMP_DIR="44444"
TCL_DUMP_DIR="55555"

function check_assigned_component()
{
    interpreter=$1

    rlAssertGrep "will-crash" "${DUMP_DIRECTORY}-${interpreter}/component"
    rlLog "component: \"$(cat ${DUMP_DIRECTORY}-${interpreter}/component)\""
    rlLog "cmdline: \"$(cat ${DUMP_DIRECTORY}-${interpreter}/cmdline)\""
}

function prepare_dump_dir()
{
    interpreter=$1

    cp -ra "$DUMP_DIRECTORY" "${DUMP_DIRECTORY}-${interpreter}"
    grep -rl '@interpreter@' "${DUMP_DIRECTORY}-${interpreter}" | xargs sed -i "s|@interpreter@|${interpreter}|g"
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

    rlPhaseStartTest "Don’t blame Python interpreter"
        generate_python3_segfault
        wait_for_hooks
        get_crash_path

        rlAssertGrep "will-crash" "$crash_PATH/component"
        rlLog "content of $crash_PATH/component file: $(cat $crash_PATH/component)"
        rlLog "content of $crash_PATH/cmdline file: $(cat $crash_PATH/cmdline)"

        remove_problem_directory
    rlPhaseEnd

    rlPhaseStartTest "Don’t blame Perl interpreter"
        PERL_VERSION="$(rpm --queryformat="%{VERSION}" -q perl-interpreter)"
        prepare_dump_dir "perl$PERL_VERSION"

        rlRun "$ABRT_SAVE_ACTION -d '${DUMP_DIRECTORY}-perl${PERL_VERSION}'" 0
        check_assigned_component "perl$PERL_VERSION"
    rlPhaseEnd

    rlPhaseStartTest "Don’t blame PHP interpreter"
        prepare_dump_dir 'php'

        rlRun "$ABRT_SAVE_ACTION -d '${DUMP_DIRECTORY}-php'" 0
        check_assigned_component 'php'

        prepare_dump_dir 'php-cgi'

        rlRun "$ABRT_SAVE_ACTION -d '${DUMP_DIRECTORY}-php-cgi'" 0
        check_assigned_component 'php-cgi'
    rlPhaseEnd

    rlPhaseStartTest "Don’t blame R interpreter"
        prepare_dump_dir 'R'

        rlRun "$ABRT_SAVE_ACTION -d '${DUMP_DIRECTORY}-R'" 0
        check_assigned_component 'R'
    rlPhaseEnd

    rlPhaseStartTest "Don’t blame Tcl interpreter"
        prepare_dump_dir "tclsh"

        rlRun "$ABRT_SAVE_ACTION -d '${DUMP_DIRECTORY}-tclsh'" 0
        check_assigned_component 'tclsh'
    rlPhaseEnd

    rlPhaseStartCleanup
        rlRun "popd"
        rlRun "rm -r $TmpDir" 0 "Removing tmp directory"
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
