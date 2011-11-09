#!/bin/bash
if [ "$SHUTDOWN" = "1" ]; then
    echo "All done, shutdown in 60 seconds"
    sleep 60
    echo "Shutdown"
    shutdown -h now
else
    echo "All done, shutdown disabled"
fi

