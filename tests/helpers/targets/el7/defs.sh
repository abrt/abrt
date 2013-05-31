#!/bin/bash

DATA_DIR=~/targets/el7
VM_NAME='EL7_nightly_run'
REPO='git://abrt.brq.redhat.com/abrt.git'
OS_VARIANT='rhel7'
KS_NAME_PREFIX='brq_el7'
LOC='http://download.englab.brq.redhat.com/pub/rhel/nightly/latest-RHEL-7/compose/Workstation/x86_64/os/'
DISK=$( echo /dev/mapper/*el7_vm )
REPOS_REQUIRED='el7_latest'
