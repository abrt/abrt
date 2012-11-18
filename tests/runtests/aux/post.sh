#!/bin/bash
if [ "$SHUTDOWN" = "1" ]; then
    echo "All done, shutting down"
    shutdown -h now
else
    echo "All done, shutdown disabled"
fi

