LPATHDIR="$HOME/.cache/abrt"
SINCEFILE="$LPATHDIR/lastnotification"

if [ ! -f "$LPATHDIR" ]; then
    mkdir -p "$LPATHDIR"
fi

TMPPATH=`mktemp --tmpdir="$LPATHDIR" lastnotification.XXXXXXXX 2> /dev/null`

SINCE=0
if [ -f "$SINCEFILE" ]; then
    SINCE=`cat $SINCEFILE 2> /dev/null`
fi

# always update the lastnotification
if [ -f "$TMPPATH" ]; then
    date +%s > "$TMPPATH"
    mv "$TMPPATH" "$SINCEFILE"
fi

abrt-cli status --since="$SINCE" 2> /dev/null

