#!/bin/bash

if [ -x /bin/systemctl ]; then
    /bin/systemctl start sendmail.service
else
    /usr/sbin/service sendmail start
fi

pushd $( cd "$(dirname $0)" && pwd )

. abrt-testing-defs.sh

yum install -y beakerlib dejagnu btparser-devel time createrepo mock

rm -rf $ABRT_TESTOUT_ROOT
mkdir -p $ABRT_TESTOUT_ROOT

testlist=$(cat test_order | grep '^[^#]\+$')

CTESTFAIL=0
for test_dir in $testlist; do
    test="$test_dir/runtest.sh"
    echo "---> $test"
    ./runner.sh $test

    TESTNAME="$(grep 'TEST=\".*\"' $test | awk -F '=' '{ print $2 }' | sed 's/"//g')"
    test_result="FAIL"
    if [ -e $ABRT_TESTOUT_ROOT/TESTOUT-${TESTNAME}.log ]; then
        if ! grep -q FAIL $ABRT_TESTOUT_ROOT/TESTOUT-${TESTNAME}.log; then
            test_result="PASS"
        fi
    fi
    echo -e "\n---> ${test_result}\n"
    if [ "$test_result" == "FAIL" ]; then
        for ctest in $TEST_CRITICAL; do
            if [ "$test" == "$ctest/runtest.sh" ]; then
                echo -n "---> Critical test failed"
                if [ "$TEST_CONTINUE" = "0" ]; then
                    echo ", stopping further testing"
                    CTESTFAIL=1
                    break
                else
                    echo ", TEST_CONTINUE in effect, resuming testing"
                fi
            fi
        done
        if [ $CTESTFAIL -eq 1 ]; then
            break
        fi
    fi
done

date="$(date +%F)"
if grep -q FAIL $ABRT_TESTOUT_ROOT/abrt-test-output.summary; then
    RESULT="FAIL"
else
    RESULT="PASS"
fi

if [ $CTESTFAIL -eq 1 ]; then
    RESULT="FAIL"
fi

tar czf $ABRT_TESTOUT_ROOT.tar.gz $ABRT_TESTOUT_ROOT
echo -n "Sending report to <$MAILTO>: "
if cat $ABRT_TESTOUT_ROOT/abrt-test-output.summary | mail -v -s "[$date] [$RESULT] ABRT testsuite report" -r $MAILFROM -a $ABRT_TESTOUT_ROOT.tar.gz $MAILTO; then
    echo "OK"
else
    echo "FAILED"
fi

popd

if [ "$SHUTDOWN" = "1" ]; then
    echo "All done, shutdown in 60 seconds"
    sleep 60
    echo "Shutdown"
    shutdown -h now
else
    echo "All done, shutdown disabled"
fi
