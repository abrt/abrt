#!/bin/bash

DATA_DIR=~/targets/rawhide
VM_NAME='RAWHIDE_nightly_run'
REPO='git://abrt.brq.redhat.com/abrt.git'
OS_VARIANT='fedora16'
KS_NAME_PREFIX='fedora_rawhide'
LOC='http://download.englab.brq.redhat.com/pub/fedora/linux/development/rawhide/x86_64/'
#LOC='http://download.englab.brq.redhat.com/pub/fedora/linux/releases/17/Fedora/x86_64/os/'

DISK=$( echo /dev/mapper/*rawhide_vm )

