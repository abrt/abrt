#!/bin/bash

DATA_DIR=~/targets/el6_3
VM_NAME='EL6.3_nightly_run'
REPO='git://abrt.brq.redhat.com/abrt.git'
OS_VARIANT='rhel6'
KS_NAME_PREFIX='brq_el6_3'
LOC='http://download.englab.brq.redhat.com/pub/rhel/nightly/latest-RHEL-6/6.3/Workstation/x86_64/os/'
DISK=$( echo /dev/mapper/*el6_3_vm )
REPOS_REQUIRED='el6_3'
