#!/bin/bash

. ./aux/lib.sh

load_abrt_conf

testlist=$(cat $TEST_LIST | grep '^[^#]\+$')
crit_test_fail=0

RESULT="PASS"

for test_dir in $testlist; do
    test="$test_dir/runtest.sh"
    testname="$(grep 'TEST=\".*\"' $test | awk -F '=' '{ print $2 }' | sed 's/"//g')"
    short_testname=${testname:0:55}
    outdir="$OUTPUT_ROOT/test/$testname"
    logfile="$outdir/full.log"

    mkdir -p $outdir

    # test start date/time
    test_start_dt=$( date +"%m/%d/%Y %H:%M:%S" )

    syslog $short_testname
    $RUNNER_SCRIPT $test &> $logfile
    syslog "End: $short_testname"

    sleep 1

    # save post crashes
    if [ -d "$ABRT_CONF_DUMP_LOCATION" ]; then
        n_post=$( find $ABRT_CONF_DUMP_LOCATION -mindepth 1 -type d | wc -l )
        if [ $n_post -gt 0 ]; then
            mkdir "$outdir/post_crashes"
            for dir in $( find $ABRT_CONF_DUMP_LOCATION  -mindepth 1 -maxdepth 1 -type d ); do
                # do not store crashes that are too big
                if [ $( du -s "$dir" | awk '{ print $1 }' ) -lt 100000 ]; then
                    mv "$dir" "$outdir/post_crashes/"
                else
                    rm -rf "$dir"
                fi
            done
        fi
    fi

    # extract test protocol
    start_line=$(grep -n -i 'Test protocol' $logfile | awk -F: '{print $1}')
    end_line=$(grep -n -i 'TEST END MARK' $logfile | awk -F: '{print $1}')

    # in case of FATAL error, there is no test protocol
    if [ "_$start_line" != "_" ]; then
        if [ $start_line -gt 1 ]; then
            start_line=$[ $start_line - 1 ]
        fi
        protocol_start=$start_line
        end_line=$[ $end_line - 1 ]

        sed -n "${start_line},${end_line}p;${end_line}q" $logfile \
            > "$outdir/protocol.log"
    fi

    # collect /var/log/messages
    start=$( grep -n "MARK: $short_testname.*" '/var/log/messages'  | tail -n 1 | awk -F: '{print $1}' )
    end=$( grep -n "MARK: End: $short_testname.*" '/var/log/messages'  | tail -n 1 | awk -F: '{print $1}' )
    start=$[ $start + 2 ]
    end=$[ $end - 2 ]

    sed -n "${start},${end}p;${end}q" '/var/log/messages' > "$outdir/messages"

    # collect dmesg
    dmesg -c > "$outdir/dmesg"

    # collect avc's
    env LC_ALL=en_US.UTF-8 ausearch -ts $test_start_dt -m avc &> "$outdir/avc"
    # don't preserve avc file if there are no matches
    if grep -q "no matches" "$outdir/avc"; then
        rm "$outdir/avc"
    fi

    # collect files stored by beakerlibs rlBundleLogs
    if stat -t /var/tmp/BEAKERLIB_STORED* &> /dev/null; then
        tmpdir=$( mktemp -d )
        tar xzf /var/tmp/BEAKERLIB_STORED* -C $tmpdir
        find $tmpdir -type f -exec mv {} $outdir \;
        rm -rf $tmpdir
        rm -f /var/tmp/BEAKERLIB_STORED*
    fi

    # check test result
    test_result="FAIL"
    if [ -e $logfile ]; then
        if ! grep -q FAIL $logfile; then
            test_result="PASS"
        fi
    fi

    # console reporting & fail.log creation
    if [ "$test_result" == "FAIL" ]; then
        touch "$outdir/fail.log"
        if [ -f "$outdir/protocol.log" ]; then
            grep -n ' FAIL ' "$outdir/protocol.log" > "$outdir/fail.log"
        else
            egrep -n ' (FAIL|FATAL) ' "$logfile" > "$outdir/fail.log"
        fi
        echo_failure
        RESULT="FAIL"
    else
        echo_success
    fi
    echo " | $short_testname"

    # append protocol to results, use fail.log if not available
    echo '' >> $OUTPUT_ROOT/results
    if [ -f "$outdir/protocol.log" ]; then
        cat "$outdir/protocol.log" >> $OUTPUT_ROOT/results
    elif [ -f "$outdir/fail.log" ]; then
        cat "$outdir/fail.log" >> $OUTPUT_ROOT/results
    fi

    if [ "$test_result" == "FAIL" ]; then
        for ctest in $TEST_CRITICAL; do
            if [ "$test" == "$ctest/runtest.sh" ]; then
                echo_failure
                echo -n " | Critical test failed"
                if [ "$TEST_CONTINUE" = "0" ]; then
                    echo ", stopping further testing"
                    crit_test_fail=1
                    break
                else
                    echo ", TEST_CONTINUE in effect, resuming testing"
                fi
            fi
        done
        if [ $crit_test_fail -eq 1 ]; then
            break
        fi
    fi

done

if [ "$RESULT" == "FAIL" ]; then
    return 1
fi
