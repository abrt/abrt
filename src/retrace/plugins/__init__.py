#!/usr/bin/python

import os

PLUGIN_DIR = "/usr/share/abrt-retrace/plugins"
PLUGINS = []

try:
    files = os.listdir(PLUGIN_DIR)
except Exception as ex:
    print "Unable to list directory '%s': %s" % (PLUGIN_DIR, ex)
    raise ImportError, ex

for filename in files:
    if not filename.startswith("_") and filename.endswith(".py"):
        pluginname = filename.replace(".py", "")
        try:
            this = __import__("%s.%s" % (__name__, pluginname))
        except:
            continue

        plugin = this.__getattribute__(pluginname)
        if plugin.__dict__.has_key("distribution") and plugin.__dict__.has_key("repos"):
            PLUGINS.append(plugin)
