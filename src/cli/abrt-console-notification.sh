# If shell is not connect to a terminal, return immediately, because this script
# should print out ABRT's status and it is senseless to continue without
# terminal.
tty -s || return 0

# Skip all for noninteractive shells for the same reason as above.
[ -z "$PS1" ] && return 0

# If $HOME is not set, a non human user is logging in to shell but this script
# should provide information to human users, therefore returning immediately
# without showing the notification.
if [ -z "$HOME" ]; then
    return 0
fi

if [ -z "$ABRT_DEBUG_LOG" ]; then
    ABRT_DEBUG_LOG="/dev/null"
fi

LPATHDIR="$HOME/.cache/abrt"
SINCEFILE="$LPATHDIR/lastnotification"

if [ ! -f "$LPATHDIR" ]; then
    # It might happen that user doesn't have write access on his home.
    mkdir -p "$LPATHDIR" >"$ABRT_DEBUG_LOG" 2>&1 || return 0
fi

TMPPATH=`mktemp --tmpdir="$LPATHDIR" lastnotification.XXXXXXXX 2> "$ABRT_DEBUG_LOG"`

SINCE=0
if [ -f "$SINCEFILE" ]; then
    SINCE=`cat $SINCEFILE 2>"$ABRT_DEBUG_LOG"`
fi

# always update the lastnotification
if [ -f "$TMPPATH" ]; then
    # Be quite in case of errors and don't scare users by strange error messages.
    date +%s > "$TMPPATH" 2>"$ABRT_DEBUG_LOG"
    mv -f "$TMPPATH" "$SINCEFILE" >"$ABRT_DEBUG_LOG" 2>&1
fi

abrt-cli status --since="$SINCE" 2>"$ABRT_DEBUG_LOG"
