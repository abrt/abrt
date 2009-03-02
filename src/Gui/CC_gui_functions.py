import gtk
try:
    # we don't want to add dependency to rpm, but if we have it, we can use it
    import rpm
except:
    rpm = None

def gui_error_message ( message, parent_dialog=None,
                      message_type=gtk.MESSAGE_ERROR,
                      widget=None, page=0, broken_widget=None ):

    dialog = gtk.MessageDialog( parent_dialog,
                               gtk.DIALOG_MODAL|gtk.DIALOG_DESTROY_WITH_PARENT,
                               message_type, gtk.BUTTONS_OK,
                               message )

    if parent_dialog:
        dialog.set_position (gtk.WIN_POS_CENTER_ON_PARENT)
        dialog.set_transient_for(parent_dialog)
    else:
        dialog.set_position (gtk.WIN_POS_CENTER)

    ret = dialog.run ()
    dialog.destroy()
    return ret
    
def gui_question_dialog ( message, parent_dialog=None,
                      message_type=gtk.MESSAGE_QUESTION,
                      widget=None, page=0, broken_widget=None ):

    dialog = gtk.MessageDialog( parent_dialog,
                               gtk.DIALOG_MODAL|gtk.DIALOG_DESTROY_WITH_PARENT,
                               message_type, gtk.BUTTONS_YES_NO,
                               message )

    if parent_dialog:
        dialog.set_position (gtk.WIN_POS_CENTER_ON_PARENT)
        dialog.set_transient_for(parent_dialog)
    else:
        dialog.set_position (gtk.WIN_POS_CENTER)

    ret = dialog.run ()
    dialog.destroy()
    return ret
    
def get_icon_for_package(theme,package):
    #print package
    try:
        return theme.load_icon(package, 22, gtk.ICON_LOOKUP_USE_BUILTIN)
    except:
        # try to find icon filename by manually
        if not rpm:
            return None
        ts = rpm.TransactionSet()
        mi = ts.dbMatch( 'name', package )
        possible_icons = []
        icon_filename = ""
        filenames = ""
        for h in mi:
            filenames = h['filenames']
        for filename in filenames:
            # add check only for last 4 chars
            if filename.rfind(".png") != -1:
                possible_icons.append(filename)
            if filename.rfind(".desktop") != -1:
                #print filename
                desktop_file = open(filename, 'r')
                lines = desktop_file.readlines()
                for line in lines:
                    if line.find("Icon=") != -1:
                        #print line[5:-1]
                        icon_filename = line[5:-1]
                        break
                desktop_file.close()
                # .dektop file found
                for filename in h['filenames']:
                    if filename.rfind("%s.png" % icon_filename) != -1:
                        #print filename
                        icon_filename = filename
                        break
            #we didn't find the .desktop file
            else:
                for filename in possible_icons:
                    if filename.rfind("%s.png" % package):
                        # return the first possible filename
                        icon_filename = filename
                        break
            if icon_filename:
                break
        if icon_filename:
            #print "icon created form %s" % icon_filename
            return gtk.gdk.pixbuf_new_from_file_at_size(icon_filename,22,22)
        else:
            return None
