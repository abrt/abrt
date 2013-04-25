#! /bin/sh

LPATH="$HOME/.cache/abrt/lastnotificaion"

SINCE=0
if [ -f $LPATH ]; then
	SINCE=`cat $LPATH`
fi

# always update the lastnotification
date +%s > "$LPATH"

abrt-cli status --since="$SINCE"
