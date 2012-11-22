#!/bin/bash

DATA_DIR=~/targets/f18
VM_NAME='F18_nightly_run'
REPO='git://abrt.brq.redhat.com/abrt.git'
OS_VARIANT='fedora16'
KS_NAME_PREFIX='fedora_18'
LOC='http://download.englab.brq.redhat.com/pub/fedora/fedora-alt/stage/18-Beta-TC6/Fedora/x86_64/os/'
DISK=$( echo /dev/mapper/*f18_vm )
MAC="52:54:00:b8:41:b4"
IP="10.34.37.129"
