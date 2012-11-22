#!/bin/bash

DATA_DIR=~/targets/f17
VM_NAME='F17_nightly_run'
REPO='git://abrt.brq.redhat.com/abrt.git'
OS_VARIANT='fedora16'
KS_NAME_PREFIX='fedora_17'
LOC='http://download.englab.brq.redhat.com/pub/fedora/linux/releases/17/Fedora/x86_64/os/'
MAC="52:54:00:40:5d:a9"
IP="10.34.33.224"

DISK=$( echo /dev/mapper/*f17_vm )

