#!/bin/bash

ABRTDIR="/etc/abrt"
LOGDIR="/var/log/abrt-retrace"
REPODIR="/var/cache/abrt-retrace"
SCRIPTDIR="/usr/share/abrt-retrace"
SRCDIR="."
WORKDIR="/var/spool/abrt-retrace"

FILES="$SRCDIR/create.wsgi $SRCDIR/status.wsgi \
       $SRCDIR/log.wsgi $SRCDIR/backtrace.wsgi \
       $SRCDIR/retrace.py $SRCDIR/abrt-retrace-reposync \
       $SRCDIR/worker.py $SRCDIR/coredump2packages.py \
       $SRCDIR/abrt-retrace-cleanup.py"

if [ ! $EUID = "0" ]
then
  echo "You must run '$0' with root permissions."
  exit 1
fi

if ! rpm -q httpd > /dev/null 2>&1
then
  echo "httpd package is required to install Retrace Server."
  exit 2
fi

if ! rpm -q mod_wsgi > /dev/null 2>&1
then
  echo "mod_wsgi package is required to install Retrace Server"
  exit 3
fi

if ! rpm -q mod_ssl > /dev/null 2>&1
then
  echo "mod_ssl package is required to install Retrace Server"
  exit 4
fi

if ! rpm -q python-webob > /dev/null 2>&1
then
  echo "python-webob package is required to install Retrace Server"
  exit 5
fi

if ! rpm -q elfutils > /dev/null 2>&1
then
  echo "elfutils package is required to install Retrace Server"
  exit 6
fi

if ! rpm -q createrepo > /dev/null 2>&1
then
  echo "createrepo package is required to install Retrace Server"
  exit 7
fi

if ! rpm -q mock > /dev/null 2>&1
then
  echo "mock package is required to install Retrace Server"
  exit 8
fi

if ! rpm -q xz > /dev/null 2>&1
then
  echo "xz package is required to install Retrace Server"
  exit 9
fi

if ! rpm -q gcc > /dev/null 2>&1
then
  echo "gcc package is required to install Retrace Server"
  exit 10
fi

if usermod -G mock root
then
  echo "User 'root' added to 'mock' group"
else
  echo "Unable to add user 'root' to group 'mock'"
  exit 11
fi

if [ ! -d "$ABRTDIR" ]
then
  if mkdir "$ABRTDIR"
  then
    echo "Created directory '$ABRTDIR'"
  else
    echo "Error creating directory '$ABRTDIR'"
    exit 12
  fi
fi

if [ ! -d "$SCRIPTDIR" ]
then
  if mkdir "$SCRIPTDIR"
  then
    echo "Created directory '$SCRIPTDIR'"
  else
    echo "Error creating directory '$SCRIPTDIR'"
    exit 13
  fi
fi

if [ ! -d "$WORKDIR" ]
then
  if mkdir "$WORKDIR"
  then
    echo "Created directory '$WORKDIR'"
    if chown apache "$WORKDIR" && chgrp apache "$WORKDIR"
    then
      echo "$WORKDIR owner and group changed to 'apache'"
    else
      echo "$WORKDIR unable to change owner or group"
      exit 14
    fi
  else
    echo "Error creating directory '$WORKDIR'"
    exit 15
  fi
fi

if [ ! -d "$REPODIR" ]
then
  if mkdir "$REPODIR"
  then
    echo "Created directory '$REPODIR'"
  else
    echo "Error creating directory '$REPODIR'"
    exit 16
  fi
fi

if [ ! -d "$LOGDIR" ]
then
  if mkdir "$LOGDIR"
  then
    echo "Created directory '$LOGDIR'"
  else
    echo "Error creating directory '$LOGDIR'"
    exit 17
  fi
fi

if ! gcc -pedantic -Wall -Wextra -Werror -o "/usr/sbin/abrt-retrace-worker" "$SRCDIR/worker.c" \
   || ! chmod u+s "/usr/sbin/abrt-retrace-worker"
then
  echo "Error compiling abrt-retrace-worker"
  exit 18
fi

echo "abrt-retrace-worker compiled"

for FILE in $FILES
do
  if cp "$FILE" "$SCRIPTDIR"
  then
    echo "Installed '$FILE'"
  else
    echo "Error installing '$FILE'"
    exit 19
  fi
done

if cp "$SRCDIR/retrace.conf" "/etc/abrt/retrace.conf"
then
  echo "Copied '$SRCDIR/retrace.conf' to '/etc/abrt/retrace.conf'"
else
  echo "Error copying '$SRCDIR/retrace.conf'"
  exit 23
fi

if cp "$SRCDIR/retrace.repo" "/etc/yum.repos.d/retrace.repo" \
   && cp "$SRCDIR/retrace-local.repo" "/etc/yum.repos.d/retrace-local.repo"
then
  echo "Copied '$SRCDIR/retrace.repo' to '/etc/yum.repos.d/retrace.repo'"
  echo "Copied '$SRCDIR/retrace-local.repo' to '/etc/yum.repos.d/retrace-local.repo'"
  echo "Running initial repository download. This will take some time."
#  "$SCRIPTDIR/abrt-retrace-reposync" fedora 14 i686
#  createrepo "$REPODIR/fedora-14-i686" > /dev/null
#  createrepo "$REPODIR/fedora-14-i686-debuginfo" > /dev/null
#  "$SCRIPTDIR/abrt-retrace-reposync" fedora 14 x86_64
#  createrepo "$REPODIR/fedora-14-x86_64" > /dev/null
#  createrepo "$REPODIR/fedora-14-x86_64-debuginfo" > /dev/null
#  "$SCRIPTDIR/abrt-retrace-reposync" fedora 15 i686
#  createrepo "$REPODIR/fedora-15-i686"
#  createrepo "$REPODIR/fedora-15-i686-debuginfo"
#  "$SCRIPTDIR/abrt-retrace-reposync" fedora 15 x86_64
#  createrepo "$REPODIR/fedora-15-x86_64"
#  createrepo "$REPODIR/fedora-15-x86_64-debuginfo"
else
  echo "Error copying '$SRCDIR/retrace.repo' or '$SRCDIR/retrace-local.repo'"
  exit 24
fi

if cp "$SRCDIR/retrace_httpd.conf" "/etc/httpd/conf.d/retrace.conf"
then
  echo "Copied '$SRCDIR/retrace_httpd.conf' to '/etc/httpd/conf.d/retrace.conf'"
  service httpd restart
else
  echo "Error copying '$SRCDIR/retrace_httpd.conf'"
  exit 25
fi

echo
echo "Retrace Server setup OK."
echo "You should set up cron to periodically synchronize local repositories. The recommended configuration is:"
echo "0 0,8,16 * * * $SCRIPTDIR/abrt-retrace-reposync fedora 14 i686"
echo "0 2,10,18 * * * $SCRIPTDIR/abrt-retrace-reposync fedora 14 x86_64"
#echo "0 4,12,20 * * * $SCRIPTDIR/abrt-retrace-reposync fedora 15 i686"
#echo "0 6,14,22 * * * $SCRIPTDIR/abrt-retrace-reposync fedora 15 x86_64"
