#!/bin/bash
for i in ~/targets/*; do
~/bin/cron_wrapper $( basename $i )
done
