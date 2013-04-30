LPATHDIR="$HOME/.cache/abrt"
SINCEFILE="$LPATHDIR/lastnotification"
TMPPATH=`mktemp --tmpdir="$LPATHDIR" lastnotification.XXXXXXXX`

SINCE=0
if [ -f $SINCEFILE ]; then
	SINCE=`cat $SINCEFILE`
fi

# always update the lastnotification
date +%s > "$TMPPATH"
mv "$TMPPATH" "$SINCEFILE"

abrt-cli status --since="$SINCE"
