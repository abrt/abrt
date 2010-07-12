from abrt_utils import _, log, log1, log2

# Doc on Gnome keyring API:
# http://library.gnome.org/devel/gnome-keyring/stable/
# Python bindings are in gnome-python2-desktop package

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

class ConfBackendLoadError(Exception):
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
# Application: abrt
# AbrtPluginInfo: Bugzilla
# NoSSLVerify: yes
# Login: user@host.com
# BugzillaURL: https://host.with.bz.com/
#
# Attributes "Application" and "AbrtPluginInfo" are special, they are used
# for efficient key retrieval via keyring API find_items_sync() function.

g_default_key_ring = None

class ConfBackendGnomeKeyring(ConfBackend):
    def __init__(self):
        global g_default_key_ring

        ConfBackend.__init__(self)
        if g_default_key_ring:
            return
        if not gkey or not gkey.is_available():
            raise ConfBackendInitError(_("Cannot connect to the Gnome Keyring daemon."))
        try:
            g_default_key_ring = gkey.get_default_keyring_sync()
        except:
            # could happen if keyring daemon is running, but we run gui under
            # user who is not the owner of the running session - using su
            raise ConfBackendInitError(_("Cannot get the default keyring."))

    def save(self, name, settings):
        settings_tmp = settings.copy()
        settings_tmp["Application"] = "abrt"
        settings_tmp["AbrtPluginInfo"] = name

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
            raise ConfBackendSaveError(_("Access to gnome-keyring has been denied, plugins settings will not be saved."))
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
        except gkey.DeniedError:
            raise ConfBackendSaveError(_("Access to gnome-keyring has been denied, plugins settings will not be saved."))

    def load(self, name):
        item_list = None
        #FIXME: make this configurable
        # this actually makes GUI to ask twice per every plugin
        # which have it's settings stored in keyring
        attempts = 2
        while attempts:
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
            except gkey.DeniedError:
                attempts -= 1
                log2("gk-authorization has failed %i time(s)", 2-attempts)
                if attempts == 0:
                    # we tried 2 times, so giving up the authorization
                    raise ConfBackendLoadError(_("Access to gnome-keyring has been denied, cannot load the settings for %s!" % name))
                continue
            break

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

        # UGLY compat cludge for users who has saved items without "Application" attr
        # (abrt <= 1.0.3 was saving those)
        item_ids = gkey.list_item_ids_sync(g_default_key_ring)
        log2("all keyring item ids:%s", item_ids)
        for item_id in item_ids:
            info = gkey.item_get_info_sync(g_default_key_ring, item_id)
            attrs = gkey.item_get_attributes_sync(g_default_key_ring, item_id)
            log2("keyring item %s: attrs:%s", item_id, str(attrs))
            if "AbrtPluginInfo" in attrs:
                if not "Application" in attrs:
                    log2("updating old-style keyring item")
                    attrs["Application"] = "abrt"
                    try:
                        gkey.item_set_attributes_sync(g_default_key_ring, item_id, attrs)
                    except:
                        log2("error updating old-style keyring item")
                plugin_name = attrs["AbrtPluginInfo"]
                # If plugin has a "Password" setting, we handle it specially: in keyring,
                # it is stored as item.secret, not as one of attributes
                if info.get_secret():
                    attrs["Password"] = info.get_secret()
                # avoiding sending useless duplicate info over dbus...
                del attrs["AbrtPluginInfo"]
                try:
                    del attrs["Application"]
                except:
                    pass
                retval[plugin_name] = attrs;
        # end of UGLY compat cludge

        try:
            log2("looking for keyring items with 'Application:abrt' attr")
            item_list = gkey.find_items_sync(gkey.ITEM_GENERIC_SECRET, { "Application": "abrt" })
        except gkey.NoMatchError:
            # nothing found
            pass
        except gkey.DeniedError:
            raise ConfBackendLoadError(_("Access to gnome-keyring has been denied, cannot load settings."))

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
            if "AbrtPluginInfo" in attrs:
                plugin_name = attrs["AbrtPluginInfo"]
                # If plugin has a "Password" setting, we handle it specially: in keyring,
                # it is stored as item.secret, not as one of attributes
                if item.secret:
                    attrs["Password"] = item.secret
                # avoiding sending useless duplicate info over dbus...
                del attrs["AbrtPluginInfo"]
                try:
                    del attrs["Application"]
                except:
                    pass
                retval[plugin_name] = attrs
        return retval


# Rudimentary backend factory

currentConfBackend = None

def getCurrentConfBackend():
    global currentConfBackend
    if not currentConfBackend:
        currentConfBackend = ConfBackendGnomeKeyring()
    return currentConfBackend
