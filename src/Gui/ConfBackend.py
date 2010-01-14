from abrt_utils import _, log, log1, log2

# Doc on Gnome keyring API:
# http://library.gnome.org/devel/gnome-keyring/stable/
# Python bindings are in gnome-python2-desktop package

#FIXME: add some backend factory

try:
    import gnomekeyring as gkey
except ImportError, e:
    gkey = None

# Exceptions
class ConfBackendInitError(Exception):
    def __init__(self, msg):
        Exception.__init__(self)
        self.what = msg

    def __str__(self):
        return self.what

class ConfBackendSaveError(Exception):
    def __init__(self, msg):
        Exception.__init__(self)
        self.what = msg

    def __str__(self):
        return self.what


class ConfBackend(object):
    def __init__(self):
        pass

    def save(self, name, settings):
        """ Default save method has to be implemented in derived class """
        raise NotImplementedError

    def load(self, name):
        """ Default load method has to be implemented in derived class """
        raise NotImplementedError


# We use Gnome keyring in the following way:
# we store passwords for each plugin in a key named "abrt:<plugin_name>".
# The value of the key becomes the value of "Password" setting.
# Other settings (if plugin has them) are stored as attributes of this key.
#
# Example: Key "abrt:Bugzilla" with bugzilla password as value, and with attributes:
#
# AbrtPluginInfo: Bugzilla
# NoSSLVerify: yes
# Login: user@host.com
# BugzillaURL: https://host.with.bz.com/
#
# The attribute "AbrtPluginInfo" is special, it is used for retrieving
# the key via keyring API find_items_sync() function.

g_default_key_ring = None

class ConfBackendGnomeKeyring(ConfBackend):
    def __init__(self):
        global g_default_key_ring

        ConfBackend.__init__(self)
        if g_default_key_ring:
            return
        if not gkey.is_available():
            raise ConfBackendInitError(_("Can't connect to Gnome Keyring daemon"))
        try:
            g_default_key_ring = gkey.get_default_keyring_sync()
        except:
            # could happen if keyring daemon is running, but we run gui under
            # user who is not the owner of the running session - using su
            raise ConfBackendInitError(_("Can't get default keyring"))

    def save(self, name, settings):
        settings_tmp = settings.copy()
        settings_tmp["AbrtPluginInfo"] = name # old way
        settings_tmp["Application"] = "abrt"
        settings_tmp["AbrtPluginName"] = name

        # delete all keyring items containg "AbrtPluginInfo":"<plugin_name>",
        # so we always have only 1 item per plugin
        try:
            item_list = gkey.find_items_sync(gkey.ITEM_GENERIC_SECRET, { "AbrtPluginInfo": str(name) })
            for item in item_list:
               log2("found old keyring item: ring:'%s' item_id:%s attrs:%s", item.keyring, item.item_id, str(item.attributes))
               log2("deleting it from keyring '%s'", g_default_key_ring)
               gkey.item_delete_sync(g_default_key_ring, item.item_id)
        except gkey.NoMatchError:
            # nothing found
            pass
        except gkey.DeniedError:
            raise ConfBackendSaveError(_("Access to gnome-keyring has been denied, plugins settings won't be saved."))
        # if plugin has a "Password" setting, we handle it specially: in keyring,
        # it is stored as item.secret, not as one of attributes
        password = ""
        if "Password" in settings_tmp:
            password = settings_tmp["Password"]
            del settings_tmp["Password"]
        # store new settings for this plugin as one keyring item
        try:
            gkey.item_create_sync(g_default_key_ring,
                                        gkey.ITEM_GENERIC_SECRET,
                                        "abrt:%s" % name, # display_name
                                        settings_tmp, # attrs
                                        password, # secret
                                        True)
        except gkey.DeniedError, e:
            raise ConfBackendSaveError(_("Access to gnome-keyring has been denied, plugins settings won't be saved."))

    def load(self, name):
        item_list = None
        try:
            log2("looking for keyring items with 'AbrtPluginInfo:%s' attr", str(name))
            item_list = gkey.find_items_sync(gkey.ITEM_GENERIC_SECRET, {"AbrtPluginInfo":str(name)})
            for item in item_list:
                # gnome keyring is weeeeird. why display_name, type, mtime, ctime
                # aren't available in find_items_sync() results? why we need to
                # get them via additional call, item_get_info_sync()?
                # internally, item has GNOME_KEYRING_TYPE_FOUND type,
                # and info has GNOME_KEYRING_TYPE_ITEM_INFO type.
                # why not use the same type for both?
                #
                # and worst of all, this information took four hours of googling...
                #
                #info = gkey.item_get_info_sync(item.keyring, item.item_id)
                log2("found keyring item: ring:'%s' item_id:%s attrs:%s", # "secret:'%s' display_name:'%s'"
                        item.keyring, item.item_id, str(item.attributes) #, item.secret, info.get_display_name()
                )
        except gkey.NoMatchError:
            # nothing found
            pass
        if item_list:
            retval = item_list[0].attributes.copy()
            retval["Password"] = item_list[0].secret
            return retval
        return {}

    # This routine loads setting for all plugins. It doesn't need plugin name.
    # Thus we can avoid talking to abrtd just in order to get plugin names.
    def load_all(self):
        retval = {}
        item_list = {}
        try:
            log2("looking for keyring items with 'Application:abrt' attr")
            item_list = gkey.find_items_sync(gkey.ITEM_GENERIC_SECRET, { "Application": "abrt" })
        except gkey.NoMatchError:
            # nothing found
            pass
        for item in item_list:
            # gnome keyring is weeeeird. why display_name, type, mtime, ctime
            # aren't available in find_items_sync() results? why we need to
            # get them via additional call, item_get_info_sync()?
            # internally, item has GNOME_KEYRING_TYPE_FOUND type,
            # and info has GNOME_KEYRING_TYPE_ITEM_INFO type.
            # why not use the same type for both?
            #
            # and worst of all, this information took four hours of googling...
            #
            #info = gkey.item_get_info_sync(item.keyring, item.item_id)
            log2("found keyring item: ring:%s item_id:%s attrs:%s", # "secret:%s display_name:'%s'"
                    item.keyring, item.item_id, str(item.attributes) #, item.secret, info.get_display_name()
            )
            attrs = item.attributes.copy()
            if "AbrtPluginName" in attrs:
                plugin_name = attrs["AbrtPluginName"]
                # If plugin has a "Password" setting, we handle it specially: in keyring,
                # it is stored as item.secret, not as one of attributes
                if item.secret:
                    attrs["Password"] = item.secret
                # avoiding sending useless duplicate info over dbus...
                del attrs["Application"]
                del attrs["AbrtPluginInfo"]
                del attrs["AbrtPluginName"]
                retval[plugin_name] = attrs;
        return retval
