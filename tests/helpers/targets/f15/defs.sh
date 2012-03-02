#!/bin/bash

DATA_DIR=~/targets/f15
VM_NAME='F15_nightly_run'
REPO='git://abrt.brq.redhat.com/abrt.git'
OS_VARIANT='fedora15'
KS_NAME_PREFIX='fedora_15'
LOC='http://download.fedoraproject.org/pub/fedora/linux/releases/15/Fedora/x86_64/os/'
DISK=$( echo /dev/mapper/*f15_vm )

