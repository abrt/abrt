date="$(date +%F)"

if [ -x /bin/systemctl ]; then
    /bin/systemctl start sendmail.service
else
    /usr/sbin/service sendmail start
fi

if grep -q 'FAIL' "$OUTPUT_ROOT/results"; then
    result='FAIL'
else
    result='PASS'
fi

echo -n "Sending report to <$MAILTO>: "
if cat $OUTPUT_ROOT/mail.summary | mail -v -s "[$date] [$result] ABRT testsuite report" -r $MAILFROM -a /tmp/report.tar.gz $MAILTO; then
    echo "OK"
else
    echo "FAILED"
fi
