#!/usr/bin/python

import re

distribution = "fedora"
abrtparser = re.compile("^Fedora release ([0-9]+) \(([^\)]+)\)$")
guessparser = re.compile("\.fc([0-9]+)")
repos = [
  [
    "rsync://ftp.sh.cvut.cz/fedora/linux/releases/$VER/Everything/$ARCH/os/Packages/*",
    "rsync://ftp.sh.cvut.cz/fedora/linux/development/$VER/$ARCH/os/Packages/*",
  ],
  [
    "rsync://ftp.sh.cvut.cz/fedora/linux/releases/$VER/Everything/$ARCH/debug/*",
    "rsync://ftp.sh.cvut.cz/fedora/linux/development/$VER/$ARCH/debug/*",
  ],
  [
    "rsync://ftp.sh.cvut.cz/fedora/linux/updates/$VER/$ARCH/*",
  ],
  [
    "rsync://ftp.sh.cvut.cz/fedora/linux/updates/$VER/$ARCH/debug/*",
  ],
  [
    "rsync://ftp.sh.cvut.cz/fedora/linux/updates/testing/$VER/$ARCH/*",
  ],
  [
    "rsync://ftp.sh.cvut.cz/fedora/linux/updates/testing/$VER/$ARCH/debug/*",
  ],
]
