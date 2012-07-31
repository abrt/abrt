#!/bin/bash

export > $OUTPUT_ROOT/pre/envs.log
cp /var/log/messages $OUTPUT_ROOT/pre/messages
dmesg -c > $OUTPUT_ROOT/pre/dmesg_pre

#yum install -y beakerlib dejagnu time createrepo mock expect
yum update abrt libreport -y

# turn off noaudit
semodule -DB
