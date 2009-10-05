from ABRTExceptions import ConfBackendInitError
from abrt_utils import _

#FIXME: add some backend factory

try:
    import gnomekeyring as gkey
except ImportError, e:
    print e
    gkey = None

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
        self.default_key_ring = gkey.get_default_keyring_sync()
        if not gkey.is_available():
            raise ConfBackendInitError(_("Can't connect do Gnome Keyring daemon"))

    def save(self, name, settings):
        print settings
        settings_tmp = settings.copy()
        settings_tmp["AbrtPluginInfo"] = name
        password = ""

        if "Password" in settings_tmp:
            print "saving password"
            password = settings_tmp["Password"]
            del settings_tmp["Password"]
        gkey.item_create_sync(self.default_key_ring,
                                    gkey.ITEM_GENERIC_SECRET,
                                    "abrt:%s" % name,
                                    settings_tmp,
                                    password,
                                    True)

    def load(self, name):
        item_list = None
        try:
            item_list = gkey.find_items_sync(gkey.ITEM_GENERIC_SECRET, {"AbrtPluginInfo":str(name)})
        except gkey.NoMatchError, ex:
            print "kekeke"
        if item_list:
            print ">>> neco"
            retval = item_list[0].attributes.copy()
            retval["Password"] = item_list[0].secret
            return retval
        else:
            return {}
            #for i in item_list:
            #    for attr in i.attributes:
            #        print attr, i.attributes[attr]
