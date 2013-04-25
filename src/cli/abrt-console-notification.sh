#! /bin/sh

LPATH="$HOME/.cache/abrt/lastnotificaion"

SINCE=0
if [ -f $LPATH ]; then
	SINCE=(`cat $LPATH`)
else
	date +%s > "$LPATH"
fi

abrt-cli status --since="$SINCE"
