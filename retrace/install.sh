#!/bin/bash

#########################################
# Retrace server test installer         #
# Michal Toman <mtoman@redhat.com>      #
#                                       #
# Requires:                             #
# root permissions                      #
# httpd installed in standard locations #
# mod_wsgi installed                    #
# xz with working --list installed      #
#########################################

SCRIPTDIR="/usr/share/abrt-retrace"
WORKDIR="/var/spool/abrt-retrace"
OWNER="apache"

if [ $EUID = "0" ]
then
    if [ ! -d $WORKDIR ]
    then
        if mkdir $WORKDIR
        then
            echo "Created directory $WORKDIR"
            if chown $OWNER $WORKDIR
            then
                echo "$WORKDIR owner changed to $OWNER"
            else
                echo "Unable to change $WORKDIR owner to $OWNER"
                exit 3
            fi
        else
            echo "Unable to create directory $WORKDIR"
            exit 2
        fi
    fi

    if [ ! -d $SCRIPTDIR ]
    then
        if mkdir $SCRIPTDIR
        then
            echo "Created directory $SCRIPTDIR"
            if chown $OWNER $SCRIPTDIR
            then
                echo "$SCRIPTDIR owner changed to $OWNER"
            else
                echo "Unable to change $SCRIPTDIR owner to $OWNER"
                exit 5
            fi
        else
            echo "Unable to create directory $SCRIPTDIR"
            exit 4
        fi
    fi

    for FILE in ./interface/*
    do
        if cp -f $FILE $SCRIPTDIR
        then
            echo "Installed $FILE"
        else
            echo "Error installing file $FILE"
            exit 6
        fi
    done

    FILE="./config/retrace.conf"
    if [ -f $FILE ]
    then
        if cp -f $FILE /etc/abrt/retrace.conf
        then
            echo "Installed $FILE"
        else
            echo "Error installing $FILE"
            exit 7
        fi
    fi

    FILE="./config/retrace_httpd.conf"
    if [ -f $FILE ]
    then
        if cp -f $FILE /etc/httpd/conf.d/retrace.conf
        then
            echo "Installed $FILE"
        else
            echo "Error installing $FILE"
            exit 8
        fi
    fi

    service httpd restart
else
    echo "You must run $0 as root."
    exit 1
fi