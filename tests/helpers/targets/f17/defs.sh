#!/bin/bash

DATA_DIR=~/targets/f17
VM_NAME='F17_nightly_run'
REPO='git://abrt.brq.redhat.com/abrt.git'
OS_VARIANT='fedora16'
KS_NAME_PREFIX='fedora_17'
#LOC='http://download.fedoraproject.org/pub/fedora/linux/development/17/x86_64/os/'
# temporary snapshot location
LOC='http://download.englab.brq.redhat.com/pub/fedora/fedora-alt/stage/17-Beta.RC2/Fedora/x86_64/os/'

DISK=$( echo /dev/mapper/*f17_vm )

