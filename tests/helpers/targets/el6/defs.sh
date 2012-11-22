#!/bin/bash

DATA_DIR=~/targets/el6/
VM_NAME='EL6_nightly_run'
REPO='git://abrt.brq.redhat.com/abrt.git'
OS_VARIANT='rhel6'
KS_NAME_PREFIX='brq_el6'
LOC='http://download.englab.brq.redhat.com/pub/rhel/rel-eng/latest-RHEL-6/6/Workstation/x86_64/os/'
DISK=$( echo /dev/mapper/*el6_vm )
REPOS_REQUIRED='el6_latest'
MAC="52:54:00:10:b4:df"
IP="10.34.37.134"
