#!/bin/bash

if [ $EUID = "0" ]
then
  if ! rpm -q httpd > /dev/null 2>&1
  then
    echo "httpd package is required to install retrace server"
    exit 1
  fi

  if ! rpm -q mod_wsgi > /dev/null 2>&1
  then
    echo "mod_wsgi package is required to install retrace server"
    exit 2
  fi

  if ! rpm -q python-webob > /dev/null 2>&1
  then
    echo "python-webob package is required to install retrace server"
    exit 3
  fi

  SCRIPTDIR="/usr/share/abrt-retrace"
  WORKDIR="/var/spool/abrt-retrace"

  if [ ! -d "$SCRIPTDIR" ]
  then
    if mkdir "$SCRIPTDIR"
    then
      echo "Created directory '$SCRIPTDIR'"
    else
      echo "Error creating directory '$SCRIPTDIR'"
      exit 4
    fi
  fi

  if [ ! -d "$WORKDIR" ]
  then
    if mkdir "$WORKDIR"
    then
      echo "Created directory '$WORKDIR'"
      if chown apache "$WORKDIR"
      then
        echo "$WORKDIR owner changed to apache"
      else
        echo "$WORKDIR unable to change owner"
      fi
    else
      echo "Error creating directory '$WORKDIR'"
      exit 5
    fi
  fi

  if ! gcc -pedantic -Wall -Wextra -Werror -o "$SCRIPTDIR/abrt-retrace-worker" "worker/worker.c" || ! chmod u+s "$SCRIPTDIR/abrt-retrace-worker"
  then
    echo "Error compiling abrt-retrace-worker"
    exit 6
  fi

  for FILE in lib/*
  do
    if cp "$FILE" "$SCRIPTDIR"
    then
      echo "Installed '$FILE'"
    else
      echo "Error installing '$FILE'"
    fi
  done

  for FILE in interface/*
  do
    if cp "$FILE" "$SCRIPTDIR"
    then
      echo "Installed '$FILE'"
    else
      echo "Error installing '$FILE'"
    fi
  done

  for FILE in worker/*
  do
    if [ "$FILE" = "worker/worker.c" ]
    then
      continue
    fi

    if cp "$FILE" "$SCRIPTDIR"
    then
      echo "Installed '$FILE'"
    else
      echo "Error installing '$FILE'"
    fi
  done

  for FILE in reposync/*
  do
    if cp "$FILE" "$SCRIPTDIR"
    then
      echo "Installed '$FILE'"
    else
      echo "Error installing '$FILE'"
    fi
  done

  if cp "config/retrace.conf" "/etc/abrt/retrace.conf"
  then
    echo "Copied 'config/retrace.conf' to '/etc/abrt/retrace.conf'"
  else
    echo "Error copying 'config/retrace.conf'"
  fi

  if cp "config/retrace_httpd.conf" "/etc/httpd/conf.d/retrace.conf"
  then
    echo "Copied 'config/retrace_httpd.conf' to '/etc/httpd/conf.d/retrace.conf'"
    service httpd restart
  else
    echo "Error copying 'config/retrace_httpd.conf'"
  fi
else
  echo "You must run '$0' as root."
fi
