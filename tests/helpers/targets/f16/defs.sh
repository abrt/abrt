#!/bin/bash

DATA_DIR=~/targets/f16
VM_NAME='F16_nightly_run'
REPO='git://abrt.brq.redhat.com/abrt.git'
OS_VARIANT='fedora16'
KS_NAME_PREFIX='fedora_16'
LOC='http://download.englab.brq.redhat.com/pub/fedora/linux/releases/16/Fedora/x86_64/os/'
DISK=$( echo /dev/mapper/*f16_vm )
MAC="52:54:00:84:6b:5b"
IP="10.34.37.156"

# http://fedoraproject.org/wiki/Anaconda/Updates
INIT_EXTRA_ARGS="updates=http://abrt.brq.redhat.com/media/updates-anaconda.img"

