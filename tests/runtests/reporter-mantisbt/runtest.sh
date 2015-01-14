#!/bin/bash
# vim: dict=/usr/share/beakerlib/dictionary.vim cpt=.,w,b,u,t,i,k
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   runtest.sh of mantisbt-reporter
#   Description: Verify reporter-mantisbt functionality
#   Author: Matej Habrnal <mhabrnal@redhat.com>
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#   Copyright (c) 2011 Red Hat, Inc. All rights reserved.
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

TEST="reporter-mantisbt"
PACKAGE="abrt"

QUERIES_DIR="."

rlJournalStart
    rlPhaseStartSetup
        LANG=""
        export LANG

        TmpDir=$(mktemp -d)
        cp -R queries/* $TmpDir
        cp -R problem_dir $TmpDir
        cp pyserve mantisbt.conf mantisbt_format.conf mantisbt_formatdup.conf $TmpDir
        cp attachment_file $TmpDir
        pushd $TmpDir
    rlPhaseEnd

    rlPhaseStartTest "sanity"
    ls -R
        rlRun "reporter-mantisbt --help &> null"
        rlRun "reporter-mantisbt --help 2>&1 | grep 'Usage:'"
    rlPhaseEnd

    # search by duphash
    # API new method for searching in MantisBT by duphas
    rlPhaseStartTest "search by duphash"
         ./pyserve login_correct search_two_issues &> server_log &

        sleep 1
        rlRun "reporter-mantisbt -vvv -h bbfe66399cc9cb8ba647414e33c5d1e4ad82b511 -c mantisbt.conf &> client_log"
        kill %1

        rlAssertGrep "<ns3:mc_login><ns3:username xsi:type=\"ns2:string\">test</ns3:username>" server_log
        rlAssertGrep "<ns3:password xsi:type=\"ns2:string\">password</ns3:password></ns3:mc_login>" server_log
        rlAssertGrep "<ns3:mc_search_issues>" server_log
        rlAssertGrep "\"abrt_hash\":\"bbfe66399cc9cb8ba647414e33c5d1e4ad82b511\"" server_log

        rlAssertGrep "<SOAP-ENV:Body><ns1:mc_loginResponse>" client_log
        rlAssertGrep "<id xsi:type=\"xsd:integer\">2</id>" client_log
        rlAssertGrep "<name xsi:type=\"xsd:string\">test</name>" client_log

        rlAssertGrep "Looking for similar problems in MantisBT" client_log
        rlAssertGrep "<item xsi:type=\"ns1:IssueData\"><id xsi:type=\"xsd:integer\">99</id>" client_log
        rlAssertGrep "99" client_log

        # not contain
        rlAssertNotGrep "Status: new open http://localhost:12345/view.php?id=99" client_log

        rm -f problem_dir/reported_to
    rlPhaseEnd

    # attach files to issue (parameter t, issue ID is specified)
    rlPhaseStartTest "attach files to issue (issue ID is specified)"
         ./pyserve \
                $QUERIES_DIR/login_correct \
                $QUERIES_DIR/attachment \
                &> server_log &
        sleep 1
        rlRun "reporter-mantisbt -vvv -c mantisbt.conf -F mantisbt_format.conf \
                -d problem_dir -t1 attachment_file &> client_log"
        kill %1

        # request
        rlAssertGrep "<ns3:mc_login><ns3:username xsi:type=\"ns2:string\">test</ns3:username>" server_log

        rlAssertGrep "<ns3:mc_issue_attachment_add>" server_log
        rlAssertGrep "<ns3:issue_id xsi:type=\"ns2:integer\">1</ns3:issue_id>" server_log
        rlAssertGrep "<ns3:name xsi:type=\"ns2:string\">attachment_file</ns3:name>" server_log
        rlAssertGrep "<ns3:content xsi:type=\"SOAP-ENC:base64\">U1NCaGJTQmhkSFJoWTJobFpDQTZLUW89Cg==</ns3:content>" server_log

        # response
        rlAssertGrep "<SOAP-ENV:Body><ns1:mc_loginResponse>" client_log
        rlAssertGrep "<id xsi:type=\"xsd:integer\">2</id>" client_log
        rlAssertGrep "name xsi:type=\"xsd:string\">test</name>" client_log

        rlAssertGrep "Attaching file 'attachment_file' to issue 1" client_log
        rlAssertGrep "<return xsi:type=\"xsd:integer\">4</return></ns1:mc_issue_attachment_addResponse>" client_log

        # not contain
        rlAssertNotGrep "Status: new open http://localhost:12345/view.php?id=99" client_log

        rm -f problem_dir/reported_to
    rlPhaseEnd

    # attach files to issue (parameter -t, issue ID is not specified)
    # API mc_issue_attachment_add
    rlPhaseStartTest "attach files to issue (issue ID is not specified)"
         ./pyserve \
                $QUERIES_DIR/login_correct \
                $QUERIES_DIR/attachment \
                &> server_log &

        rlRun "echo \"MantisBT: URL=http://localhost:12345/mantisbt/view.php?id=1\" > problem_dir/reported_to"

        sleep 1
        rlRun "reporter-mantisbt -vvv -c mantisbt.conf -F mantisbt_format.conf -d problem_dir -t attachment_file &> client_log"
        kill %1

        # request
        rlAssertGrep "<ns3:mc_issue_attachment_add>" server_log
        rlAssertGrep "<ns3:issue_id xsi:type=\"ns2:integer\">1</ns3:issue_id>" server_log
        rlAssertGrep "<ns3:name xsi:type=\"ns2:string\">attachment_file</ns3:name>" server_log

        # response
        rlAssertGrep "<SOAP-ENV:Body><ns1:mc_loginResponse>" client_log
        rlAssertGrep "<id xsi:type=\"xsd:integer\">2</id>" client_log
        rlAssertGrep "name xsi:type=\"xsd:string\">test</name>" client_log

        rlAssertGrep "Attaching file 'attachment_file' to issue 1" client_log
        rlAssertGrep "<return xsi:type=\"xsd:integer\">4</return></ns1:mc_issue_attachment_addResponse>" client_log

        # not contain
        rlAssertNotGrep "Status: new open http://localhost:12345/view.php?id=99" client_log

        rm -f problem_dir/reported_to
    rlPhaseEnd

    # force reporting even if this problem is already reported (parameter -f)
    rlPhaseStartTest "force reporting"
         ./pyserve \
                $QUERIES_DIR/login_correct \
                $QUERIES_DIR/search_two_issues \
                $QUERIES_DIR/search_no_issue \
                $QUERIES_DIR/create \
                $QUERIES_DIR/attachment \
                $QUERIES_DIR/attachment \
                &> server_log &

        # is reported
        rlRun "echo \"MantisBT: URL=http://localhost:12345/mantisbt/view.php?id=1\" > problem_dir/reported_to"

        sleep 1
        rlRun "reporter-mantisbt -f -vvv -c mantisbt.conf -F mantisbt_format.conf -A mantisbt_formatdup.conf -d problem_dir >client_log 2>&1 "
        kill %1

        #request
        rlAssertGrep "<ns1:Body><ns3:mc_login><ns3:username xsi:type=\"ns2:string\">test</ns3:username>" server_log
        rlAssertGrep "<ns3:mc_search_issues>" server_log
        rlAssertGrep "<ns3:mc_issue_add>" server_log
        rlAssertGrep "<ns3:name xsi:type=\"ns2:string\">proj</ns3:name>" server_log
        rlAssertGrep "<ns3:view_state xsi:type=\"ns3:ObjectRef\"><ns3:name xsi:type=\"ns2:string\">public</ns3:name></ns3:view_state>" server_log
        rlAssertGrep "<ns3:os_build xsi:type=\"ns2:string\">666</ns3:os_build>" server_log
        rlAssertGrep "<ns3:summary xsi:type=\"ns2:string\">\[abrt\] : rxvt_term::selection_delimit_word(): Process /usr/bin/urxvtd was killed by signal 11 (SIGSEGV)</ns3:summary>" server_log
        rlAssertGrep "<ns3:mc_issue_attachment_add>" server_log
        rlAssertGrep "<ns3:name xsi:type=\"ns2:string\">backtrace</ns3:name>" server_log

        rlAssertGrep "<ns3:issue_id xsi:type=\"ns2:integer\">7</ns3:issue_id>" server_log
        rlAssertGrep "<ns3:name xsi:type=\"ns2:string\">backtrace</ns3:name>" server_log

        #response
        rlAssertGrep "<SOAP-ENV:Body><ns1:mc_loginResponse>" client_log
        rlAssertGrep "<id xsi:type=\"xsd:integer\">2</id>" client_log
        rlAssertGrep "name xsi:type=\"xsd:string\">test</name>" client_log

        rlAssertGrep "Checking for duplicates" client_log
        rlAssertGrep "MantisBT has 2 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511' including cross-version ones" client_log
        rlAssertGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[2\]\" xsi:type=\"SOAP-ENC:Array\">" client_log
        rlAssertGrep "Potential duplicate: issue 99" client_log
        rlAssertGrep "MantisBT has 0 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511'" client_log
        rlAssertGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[0\]\" xsi:type=\"SOAP-ENC:Array\">" client_log

        rlAssertGrep "Creating a new issue" client_log
        rlAssertGrep "<ns1:mc_issue_addResponse><return xsi:type=\"xsd:integer\">7</return>" client_log

        rm -f problem_dir/reported_to
    rlPhaseEnd

    # create a new issue (only potential duplicate issues exist)
    rlPhaseStartTest "create an issue (only potential duplicate issues exist)"
         ./pyserve \
                $QUERIES_DIR/login_correct \
                $QUERIES_DIR/search_two_issues \
                $QUERIES_DIR/search_no_issue \
                $QUERIES_DIR/create \
                $QUERIES_DIR/attachment \
                $QUERIES_DIR/attachment \
                &> server_log &

        # is reported
        rlRun "echo \"MantisBT: URL=http://localhost:12345/mantisbt/view.php?id=1\" > problem_dir/reported_to"

        sleep 1
        rlRun "reporter-mantisbt -f -vvv -c mantisbt.conf -F mantisbt_format.conf -A mantisbt_formatdup.conf -d problem_dir >client_log 2>&1 "
        kill %1

        #request
        rlAssertGrep "<ns1:Body><ns3:mc_login><ns3:username xsi:type=\"ns2:string\">test</ns3:username>" server_log
        rlAssertGrep "<ns3:mc_search_issues>" server_log
        rlAssertGrep "<ns3:mc_issue_add>" server_log
        rlAssertGrep "<ns3:name xsi:type=\"ns2:string\">proj</ns3:name>" server_log
        rlAssertGrep "<ns3:view_state xsi:type=\"ns3:ObjectRef\"><ns3:name xsi:type=\"ns2:string\">public</ns3:name></ns3:view_state>" server_log
        rlAssertGrep "<ns3:os_build xsi:type=\"ns2:string\">666</ns3:os_build>" server_log
        rlAssertGrep "<ns3:summary xsi:type=\"ns2:string\">\[abrt\] : rxvt_term::selection_delimit_word(): Process /usr/bin/urxvtd was killed by signal 11 (SIGSEGV)</ns3:summary>" server_log
        rlAssertGrep "<ns3:mc_issue_attachment_add>" server_log
        rlAssertGrep "<ns3:name xsi:type=\"ns2:string\">backtrace</ns3:name>" server_log

        rlAssertGrep "<ns3:issue_id xsi:type=\"ns2:integer\">7</ns3:issue_id>" server_log
        rlAssertGrep "<ns3:name xsi:type=\"ns2:string\">backtrace</ns3:name>" server_log

        #response
        rlAssertGrep "<SOAP-ENV:Body><ns1:mc_loginResponse>" client_log
        rlAssertGrep "<id xsi:type=\"xsd:integer\">2</id>" client_log
        rlAssertGrep "name xsi:type=\"xsd:string\">test</name>" client_log

        rlAssertGrep "Checking for duplicates" client_log
        rlAssertGrep "MantisBT has 2 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511' including cross-version ones" client_log
        rlAssertGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[2\]\" xsi:type=\"SOAP-ENC:Array\">" client_log
        rlAssertGrep "Potential duplicate: issue 99" client_log
        rlAssertGrep "MantisBT has 0 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511'" client_log
        rlAssertGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[0\]\" xsi:type=\"SOAP-ENC:Array\">" client_log

        rlAssertGrep "Creating a new issue" client_log
        rlAssertGrep "<ns1:mc_issue_addResponse><return xsi:type=\"xsd:integer\">7</return>" client_log

        rlAssertGrep "Status: new http://localhost:12345/view.php?id=7" client_log

        rm -f problem_dir/reported_to
    rlPhaseEnd

    # create a new issue (no potential duplicate issues exist)
    rlPhaseStartTest "create an issue (no potential duplicate issues exist)"
         ./pyserve \
                $QUERIES_DIR/login_correct \
                $QUERIES_DIR/search_no_issue \
                $QUERIES_DIR/create \
                $QUERIES_DIR/attachment \
                $QUERIES_DIR/attachment \
                &> server_log &

        sleep 1
        rlRun "reporter-mantisbt -vvv -c mantisbt.conf -F mantisbt_format.conf -d problem_dir &> client_log"
        kill %1

        #request
        rlAssertGrep "<ns3:mc_login>" server_log
        rlAssertGrep "<ns3:mc_search_issues>" server_log
        rlAssertGrep "<ns3:mc_issue_add>" server_log
        rlAssertGrep "<ns3:mc_issue_attachment_add>" server_log

        #response
        rlAssertGrep "<SOAP-ENV:Body><ns1:mc_loginResponse>" client_log
        rlAssertGrep "<id xsi:type=\"xsd:integer\">2</id>" client_log
        rlAssertGrep "name xsi:type=\"xsd:string\">test</name>" client_log

        rlAssertGrep "Checking for duplicates" client_log
        rlAssertGrep "MantisBT has 0 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511' including cross-version ones" client_log
        rlAssertGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[0\]\" xsi:type=\"SOAP-ENC:Array\">" client_log

        # not contain
        rlAssertNotGrep "MantisBT has 2 reports" client_log #not

        rlAssertGrep "Creating a new issue" client_log
        rlAssertGrep "<ns1:mc_issue_addResponse><return xsi:type=\"xsd:integer\">7</return>" client_log

        rlAssertGrep "Status: new http://localhost:12345/view.php?id=7" client_log

        rm -f problem_dir/reported_to
    rlPhaseEnd

    # duplicate issue exist (comment doesn't exist, not closed as duplicate)
    rlPhaseStartTest "duplicate issue exist (comment doesn't exist)"
         ./pyserve \
                $QUERIES_DIR/login_correct \
                $QUERIES_DIR/search_two_issues \
                $QUERIES_DIR/search_one_issue \
                $QUERIES_DIR/get_issue \
                &> server_log &

        rlRun "rm -f problem_dir/comment"

        sleep 1
        rlRun "reporter-mantisbt -vvv -c mantisbt.conf -F mantisbt_format.conf -A mantisbt_formatdup.conf -d problem_dir &> client_log"
        kill %1

        # request
        rlAssertGrep "<ns3:mc_login>" server_log
        rlAssertGrep "<ns3:mc_search_issues>" server_log
        rlAssertGrep "<ns3:mc_issue_get>" server_log
        rlAssertGrep "<ns3:issue_id xsi:type=\"ns2:integer\">99</ns3:issue_id></ns3:mc_issue_get>" server_log

        # response
        rlAssertGrep "<SOAP-ENV:Body><ns1:mc_loginResponse>" client_log
        rlAssertGrep "<id xsi:type=\"xsd:integer\">2</id>" client_log
        rlAssertGrep "name xsi:type=\"xsd:string\">test</name>" client_log

        rlAssertGrep "Checking for duplicates" client_log
        rlAssertGrep "MantisBT has 2 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511' including cross-version ones" client_log
        rlAssertGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[2\]\" xsi:type=\"SOAP-ENC:Array\">" client_log
        rlAssertGrep "MantisBT has 1 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511'" client_log
        rlAssertNotGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[0\]\" xsi:type=\"SOAP-ENC:Array\">" client_log #not

        rlAssertGrep "<ns1:mc_issue_getResponse><return xsi:type=\"ns1:IssueData\"><id xsi:type=\"xsd:integer\">99</id>" client_log
        rlAssertGrep "<name xsi:type=\"xsd:string\">new</name></status>" client_log
        rlAssertGrep "Bug is already reported: 99" client_log

        rlAssertGrep "Status: new open http://localhost:12345/view.php?id=99" client_log

        rm -f problem_dir/reported_to
    rlPhaseEnd

    # duplicate issue exist (comment doesn't exist, closed as duplicate)
    rlPhaseStartTest "duplicate issue exist (comment doesn't exist)"
         ./pyserve \
                $QUERIES_DIR/login_correct \
                $QUERIES_DIR/search_two_issues \
                $QUERIES_DIR/search_one_issue \
                $QUERIES_DIR/get_issue_closed_as_duplicate \
                $QUERIES_DIR/get_issue \
                &> server_log &

        sleep 1
        rlRun "reporter-mantisbt -vvv -c mantisbt.conf -F mantisbt_format.conf -A mantisbt_formatdup.conf -d problem_dir &> client_log"
        kill %1

        # request
        rlAssertGrep "<ns3:mc_login>" server_log
        rlAssertGrep "<ns3:mc_search_issues>" server_log
        rlAssertGrep "<ns3:mc_issue_get>" server_log
        rlAssertGrep "<ns3:issue_id xsi:type=\"ns2:integer\">99</ns3:issue_id></ns3:mc_issue_get>" server_log

        # response
        rlAssertGrep "<SOAP-ENV:Body><ns1:mc_loginResponse>" client_log
        rlAssertGrep "<id xsi:type=\"xsd:integer\">2</id>" client_log
        rlAssertGrep "name xsi:type=\"xsd:string\">test</name>" client_log

        rlAssertGrep "Checking for duplicates" client_log
        rlAssertGrep "MantisBT has 2 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511' including cross-version ones" client_log
        rlAssertGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[2\]\" xsi:type=\"SOAP-ENC:Array\">" client_log
        rlAssertGrep "MantisBT has 1 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511'" client_log

        # not contain
        rlAssertNotGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[0\]\" xsi:type=\"SOAP-ENC:Array\">" client_log

        rlAssertGrep "<ns1:mc_issue_getResponse><return xsi:type=\"ns1:IssueData\"><id xsi:type=\"xsd:integer\">99</id>" client_log
        rlAssertGrep "<name xsi:type=\"xsd:string\">new</name></status>" client_log
        rlAssertGrep "<name xsi:type=\"xsd:string\">duplicate</name></resolution>" client_log
        rlAssertGrep "Bug is already reported: 99" client_log
        rlAssertGrep "Issue 99 is a duplicate, using parent issue 101" client_log

        rlAssertGrep "Status: new open http://localhost:12345/view.php?id=101" client_log
        # not contain
        rlAssertNotGrep "Status: new open http://localhost:12345/view.php?id=99" client_log

        rm -f problem_dir/reported_to
        rm -f problem_dir/comment

    rlPhaseEnd

    # duplicate issue exist (comment file exists, isn't duplicate, attach backtrace)
    rlPhaseStartTest "duplicate issue exist (comment file exists, isn't duplicate, attach backtrace)"
         ./pyserve \
                $QUERIES_DIR/login_correct \
                $QUERIES_DIR/search_two_issues \
                $QUERIES_DIR/search_one_issue \
                $QUERIES_DIR/get_issue \
                $QUERIES_DIR/add_note \
                $QUERIES_DIR/attachment \
                &> server_log &

        # create a comment file
        rlRun "echo \"i am comment\" > problem_dir/comment"

        sleep 1
        rlRun "reporter-mantisbt -vvv -c mantisbt.conf -F mantisbt_format.conf -A mantisbt_formatdup.conf -d problem_dir &> client_log"
        kill %1

        # request
        rlAssertGrep "<ns3:mc_login>" server_log
        rlAssertGrep "<ns3:mc_search_issues>" server_log
        rlAssertGrep "<ns3:mc_issue_get>" server_log
        rlAssertGrep "<ns3:issue_id xsi:type=\"ns2:integer\">99</ns3:issue_id></ns3:mc_issue_get>" server_log
        rlAssertGrep "<ns3:mc_issue_note_add>" server_log
        rlAssertGrep "<ns3:mc_issue_attachment_add>" server_log

        # response
        rlAssertGrep "<SOAP-ENV:Body><ns1:mc_loginResponse>" client_log
        rlAssertGrep "<id xsi:type=\"xsd:integer\">2</id>" client_log
        rlAssertGrep "name xsi:type=\"xsd:string\">test</name>" client_log

        rlAssertGrep "Checking for duplicates" client_log
        rlAssertGrep "MantisBT has 2 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511' including cross-version ones" client_log
        rlAssertGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[2\]\" xsi:type=\"SOAP-ENC:Array\">" client_log
        rlAssertGrep "MantisBT has 1 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511'" client_log
        rlAssertNotGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[0\]\" xsi:type=\"SOAP-ENC:Array\">" client_log #not

        rlAssertGrep "<ns1:mc_issue_getResponse><return xsi:type=\"ns1:IssueData\"><id xsi:type=\"xsd:integer\">99</id>" client_log
        rlAssertGrep "<name xsi:type=\"xsd:string\">new</name></status>" client_log
        rlAssertGrep "Bug is already reported: 99" client_log
        rlAssertGrep "Adding new comment to issue 99" client_log
        rlAssertGrep "Attaching better backtrace" client_log
        rlAssertGrep "<ns1:mc_issue_note_addResponse><return xsi:type=\"xsd:integer\">5</return></ns1:mc_issue_note_addResponse>" client_log
        rlAssertGrep "<return xsi:type=\"xsd:integer\">4</return></ns1:mc_issue_attachment_addResponse>" client_log

        rlAssertGrep "Status: new open http://localhost:12345/view.php?id=99" client_log

        rm -f problem_dir/reported_to
        rm -f problem_dir/comment
    rlPhaseEnd

    # duplicate issue exist (comment file exists, is duplicate)
    rlPhaseStartTest "duplicate issue exist (comment file exists, is duplicate)"
         ./pyserve \
                $QUERIES_DIR/login_correct \
                $QUERIES_DIR/search_two_issues \
                $QUERIES_DIR/search_one_issue \
                $QUERIES_DIR/get_issue_dup_comment_rating_1 \
                &> server_log &

        # create a comment file
        rlRun "echo \"i am comment\" > problem_dir/comment"

        sleep 1
        rlRun "reporter-mantisbt -vvv -c mantisbt.conf -F mantisbt_format.conf -A mantisbt_formatdup.conf -d problem_dir &> client_log"
        kill %1

        # request
        rlAssertGrep "<ns3:mc_login>" server_log
        rlAssertGrep "<ns3:mc_search_issues>" server_log
        rlAssertGrep "<ns3:mc_issue_get>" server_log
        rlAssertGrep "<ns3:issue_id xsi:type=\"ns2:integer\">99</ns3:issue_id></ns3:mc_issue_get>" server_log

        # response
        rlAssertGrep "<SOAP-ENV:Body><ns1:mc_loginResponse>" client_log
        rlAssertGrep "<id xsi:type=\"xsd:integer\">2</id>" client_log
        rlAssertGrep "name xsi:type=\"xsd:string\">test</name>" client_log

        rlAssertGrep "Checking for duplicates" client_log
        rlAssertGrep "MantisBT has 2 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511' including cross-version ones" client_log
        rlAssertGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[2\]\" xsi:type=\"SOAP-ENC:Array\">" client_log
        rlAssertGrep "MantisBT has 1 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511'" client_log

        # not contain
        rlAssertNotGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[0\]\" xsi:type=\"SOAP-ENC:Array\">" client_log

        rlAssertGrep "<ns1:mc_issue_getResponse><return xsi:type=\"ns1:IssueData\"><id xsi:type=\"xsd:integer\">99</id>" client_log
        rlAssertGrep "<name xsi:type=\"xsd:string\">new</name></status>" client_log
        rlAssertGrep "Bug is already reported: 99" client_log
        rlAssertGrep "Found the same comment in the issue history, not adding a new one" client_log

        # not contain
        rlAssertNotGrep "<ns1:mc_issue_note_addResponse><return xsi:type=\"xsd:integer\">5</return></ns1:mc_issue_note_addResponse>" client_log
        rlAssertNotGrep "<return xsi:type=\"xsd:integer\">4</return></ns1:mc_issue_attachment_addResponse>" client_log

        rlAssertGrep "Status: new open http://localhost:12345/view.php?id=99" client_log

        rm -f problem_dir/reported_to
        rm -f problem_dir/comment
    rlPhaseEnd

    rlPhaseStartTest "duplicate issue exist (comment file exists, isn't duplicate, the same backtrace rating)"
         ./pyserve \
                $QUERIES_DIR/login_correct \
                $QUERIES_DIR/search_two_issues \
                $QUERIES_DIR/search_one_issue \
                $QUERIES_DIR/get_issue_comment_rating_1 \
                $QUERIES_DIR/add_note \
                &> server_log &

        # create a comment file
        rlRun "echo \"i am comment\" > problem_dir/comment"

        sleep 1
        rlRun "reporter-mantisbt -vvv -c mantisbt.conf -F mantisbt_format.conf -A mantisbt_formatdup.conf -d problem_dir &> client_log"
        kill %1

        # request
        rlAssertGrep "<ns3:mc_login>" server_log
        rlAssertGrep "<ns3:mc_search_issues>" server_log
        rlAssertGrep "<ns3:mc_issue_get>" server_log
        rlAssertGrep "<ns3:issue_id xsi:type=\"ns2:integer\">99</ns3:issue_id></ns3:mc_issue_get>" server_log
        rlAssertGrep "<ns3:mc_issue_note_add>" server_log

        #not contain
        rlAssertNotGrep "<ns3:mc_issue_attachment_add>" server_log

        # response
        rlAssertGrep "<SOAP-ENV:Body><ns1:mc_loginResponse>" client_log
        rlAssertGrep "<id xsi:type=\"xsd:integer\">2</id>" client_log
        rlAssertGrep "name xsi:type=\"xsd:string\">test</name>" client_log

        rlAssertGrep "Checking for duplicates" client_log
        rlAssertGrep "MantisBT has 2 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511' including cross-version ones" client_log
        rlAssertGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[2\]\" xsi:type=\"SOAP-ENC:Array\">" client_log
        rlAssertGrep "MantisBT has 1 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511'" client_log

        # not contain
        rlAssertNotGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[0\]\" xsi:type=\"SOAP-ENC:Array\">" client_log 

        rlAssertGrep "<ns1:mc_issue_getResponse><return xsi:type=\"ns1:IssueData\"><id xsi:type=\"xsd:integer\">99</id>" client_log
        rlAssertGrep "<name xsi:type=\"xsd:string\">new</name></status>" client_log
        rlAssertGrep "Bug is already reported: 99" client_log
        rlAssertGrep "<ns1:mc_issue_note_addResponse><return xsi:type=\"xsd:integer\">5</return></ns1:mc_issue_note_addResponse>" client_log
        rlAssertGrep "Adding new comment to issue 99" client_log

        # not contain
        rlAssertNotGrep "Attaching better backtrace" client_log
        rlAsserNottGrep "Found the same comment in the issue history, not adding a new one" client_log
        rlAssertNotGrep "<return xsi:type=\"xsd:integer\">4</return></ns1:mc_issue_attachment_addResponse>" client_log

        rlAssertGrep "Status: new open http://localhost:12345/view.php?id=99" client_log

        rm -f problem_dir/reported_to
        rm -f problem_dir/comment
    rlPhaseEnd

    rlPhaseStartTest "duplicate issue exist (comment file exists, isn't duplicate, better backtrace rating)"
         ./pyserve \
                $QUERIES_DIR/login_correct \
                $QUERIES_DIR/search_two_issues \
                $QUERIES_DIR/search_one_issue \
                $QUERIES_DIR/get_issue_comment_rating_1 \
                $QUERIES_DIR/add_note \
                $QUERIES_DIR/attachment \
                &> server_log &

        # create a comment file
        rlRun "echo \"i am comment\" > problem_dir/comment"
        rlRun "echo \"5\" > problem_dir/backtrace_rating"

        sleep 1
        rlRun "reporter-mantisbt -vvv -c mantisbt.conf -F mantisbt_format.conf -A mantisbt_formatdup.conf -d problem_dir &> client_log"
        kill %1

        # request
        rlAssertGrep "<ns3:mc_login>" server_log
        rlAssertGrep "<ns3:mc_search_issues>" server_log
        rlAssertGrep "<ns3:mc_issue_get>" server_log
        rlAssertGrep "<ns3:issue_id xsi:type=\"ns2:integer\">99</ns3:issue_id></ns3:mc_issue_get>" server_log
        rlAssertGrep "<ns3:mc_issue_note_add>" server_log
        rlAssertGrep "<ns3:mc_issue_attachment_add>" server_log

        # response
        rlAssertGrep "<SOAP-ENV:Body><ns1:mc_loginResponse>" client_log
        rlAssertGrep "<id xsi:type=\"xsd:integer\">2</id>" client_log
        rlAssertGrep "name xsi:type=\"xsd:string\">test</name>" client_log

        rlAssertGrep "Checking for duplicates" client_log
        rlAssertGrep "MantisBT has 2 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511' including cross-version ones" client_log
        rlAssertGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[2\]\" xsi:type=\"SOAP-ENC:Array\">" client_log

        # not contain
        rlAssertGrep "MantisBT has 1 reports with duphash 'bbfe66399cc9cb8ba647414e33c5d1e4ad82b511'" client_log
        rlAssertNotGrep "<return SOAP-ENC:arrayType=\"ns1:IssueData\[0\]\" xsi:type=\"SOAP-ENC:Array\">" client_log 

        rlAssertGrep "<ns1:mc_issue_getResponse><return xsi:type=\"ns1:IssueData\"><id xsi:type=\"xsd:integer\">99</id>" client_log
        rlAssertGrep "<name xsi:type=\"xsd:string\">new</name></status>" client_log
        rlAssertGrep "Bug is already reported: 99" client_log
        rlAssertGrep "<ns1:mc_issue_note_addResponse><return xsi:type=\"xsd:integer\">5</return></ns1:mc_issue_note_addResponse>" client_log
        rlAssertGrep "Adding new comment to issue 99" client_log
        rlAssertGrep "Attaching better backtrace" client_log
        rlAssertGrep "<return xsi:type=\"xsd:integer\">4</return></ns1:mc_issue_attachment_addResponse>" client_log

        rlAssertGrep "Status: new open http://localhost:12345/view.php?id=99" client_log

        # not contain
        rlAsserNottGrep "Found the same comment in the issue history, not adding a new one" client_log

        rm -f problem_dir/reported_to
        rm -f problem_dir/comment
        rlRun "echo \"1\" > problem_dir/backtrace_rating"
    rlPhaseEnd

    rlPhaseStartCleanup
        rlBundleLogs abrt server* client*
        popd # TmpDir
        rm -rf $TmpDir
    rlPhaseEnd
    rlJournalPrintText
rlJournalEnd
