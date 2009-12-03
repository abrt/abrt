from abrt_utils import _

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


class ConfBackendGnomeKeyring(ConfBackend):
    def __init__(self):
        ConfBackend.__init__(self)
        if not gkey.is_available():
            raise ConfBackendInitError(_("Can't connect to Gnome Keyring daemon"))
        try:
            self.default_key_ring = gkey.get_default_keyring_sync()
        except:
            # could happen if keyring daemon is running, but we run gui under
            # user who is not owner is the running session - using su
            raise ConfBackendInitError(_("Can't get default keyring"))

    def save(self, name, settings):
        settings_tmp = settings.copy()
        settings_tmp["AbrtPluginInfo"] = name
        password = ""

        item_list = []
        try:
            item_list = gkey.find_items_sync(gkey.ITEM_GENERIC_SECRET, {"AbrtPluginInfo":str(name)})
        except gkey.NoMatchError:
            # nothing found
            pass
        except gkey.DeniedError:
            raise ConfBackendSaveError(_("Acces to gnome-keyring has been denied, plugins settings won't be saved."))

        # delete all items containg "AbrtPluginInfo":<plugin_name>, so we always have only 1 item per plugin
        for item in item_list:
            gkey.item_delete_sync(self.default_key_ring, item.item_id)

        if "Password" in settings_tmp:
            password = settings_tmp["Password"]
            del settings_tmp["Password"]
        try:
            gkey.item_create_sync(self.default_key_ring,
                                        gkey.ITEM_GENERIC_SECRET,
                                        "abrt:%s" % name,
                                        settings_tmp,
                                        password,
                                        True)
        except gkey.DeniedError, e:
            raise ConfBackendSaveError(_("Acces to gnome-keyring has been denied, plugins settings won't be saved."))

    def load(self, name):
        item_list = None
        try:
            item_list = gkey.find_items_sync(gkey.ITEM_GENERIC_SECRET, {"AbrtPluginInfo":str(name)})
        except gkey.NoMatchError:
            # nothing found
            pass

        if item_list:
            retval = item_list[0].attributes.copy()
            retval["Password"] = item_list[0].secret
            return retval
        else:
            return {}
            #for i in item_list:
            #    for attr in i.attributes:
            #        print attr, i.attributes[attr]
