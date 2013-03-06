#!/bin/sh

test "$1" || { echo "Need a fedorahosted account name"; exit 1; }

user="$1"
shift

test "$1" || { echo "Need a file to upload"; exit 1; }
test -f "$1" || { echo "Not a file: '$1'"; exit 1; }

scp "$1" "$user"@fedorahosted.org:abrt
