
pushd $OUTPUT_ROOT

touch 'mail.summary'
if grep -q 'FAIL' 'results'; then
    echo 'Failed tests:' >> 'mail.summary'
    for ft in $( find . -type f -name 'fail.log' ); do
        dir=$( dirname $ft )
        ts=$( basename $dir )

        echo " - $ts:" >> 'mail.summary'
        cat "$dir/fail.log" >> 'mail.summary'
        echo "" >> 'mail.summary'
    done
else
    echo 'All tests passed' >> 'mail.summary'
fi
popd

pushd $( dirname $OUTPUT_ROOT )
tar czvf /tmp/report.tar.gz $( basename $OUTPUT_ROOT )
popd
