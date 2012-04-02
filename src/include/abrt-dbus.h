#ifndef ABRTDBUS_H_
#define ABRTDBUS_H_

#define ABRT_DBUS_NAME      "org.freedesktop.problems"
#define ABRT_DBUS_OBJECT     "/org/freedesktop/problems"
#define ABRT_DBUS_IFACE    "org.freedesktop.problems"

GList *string_list_from_variant(GVariant *variant);

GVariant *variant_from_string_list(GList *strings);

#endif /* ABRTDBUS_H_ */
