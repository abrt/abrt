#include <gio/gio.h>

#include "abrt-polkit.h"
#include "libabrt.h"

#define ABRT_CONF_DBUS_NAME "com.redhat.problems.configuration"
#define ABRT_CONF_IFACE_PFX ABRT_CONF_DBUS_NAME

#ifndef PROBLEMS_CONFIG_INTERFACES_DIR
# error "Undefined PROBLEMS_CONFIG_INTERFACES_DIR"
#endif

#define ANNOTATION_NAME_CONF_FILE "com.redhat.problems.ConfFile"
#define ANNOTATION_NAME_DEF_CONF_FILE "com.redhat.problems.DefaultConfFile"

#define ABRT_FILE_ACCESS_ERROR abrt_file_access_error_quark()
#define ABRT_FILE_ACCESS_ERROR_CODE ENOENT

static GQuark abrt_file_access_error_quark()
{
    return g_quark_from_static_string("abrt-file-access-error");
}

#define ABRT_REFLECTION_UNSUPPORTED_TYPE_ERROR abrt_reflection_unsupported_type_error_quark()
#define ABRT_REFLECTION_UNSUPPORTED_TYPE_ERROR_CODE (129)

static GQuark abrt_reflection_unsupported_type_error_quark()
{
    return g_quark_from_static_string("abrt-reflection-unsupported-type-error");
}

#define ABRT_REFLECTION_MISSING_MEMBER_ERROR abrt_reflection_missing_member_error_quark()
#define ABRT_REFLECTION_MISSING_MEMBER_ERROR_CODE (130)

static GQuark abrt_reflection_missing_member_error_quark()
{
    return g_quark_from_static_string("abrt-reflection-missing-member-error");
}

#define ABRT_FILE_FORMAT_ERROR abrt_file_fromat_error_quark()
#define ABRT_FILE_FORMAT_ERROR_CODE (131)

static GQuark abrt_file_fromat_error_quark()
{
    return g_quark_from_static_string("abrt-file-format-error");
}

#define ABRT_AUTHORIZATION_ERROR abrt_authorization_error_quark()
#define ABRT_AUTHORIZATION_ERROR_CODE (132)

static GQuark abrt_authorization_error_quark()
{
    return g_quark_from_static_string("abrt-authorization-error");
}


const char *g_default_xml =
"<node><interface name=\"com.redhat.problems.configuration\"><method name=\"SetDefault\"><arg name=\"name\" type=\"s\" direction=\"in\" /></method></interface></node>";

GDBusNodeInfo *g_default_node;

guint g_timeout_source;
int g_timeout_value = 10;
GMainLoop *g_loop;

static gboolean on_timeout_cb(gpointer user_data)
{
    log_info("Inactivity timeout was reached");
    g_main_loop_quit(g_loop);
    return TRUE;
}

static void reset_timeout(void)
{
    if (g_timeout_source > 0)
    {
        log_info("Removing timeout");
        g_source_remove(g_timeout_source);
    }
    log_info("Setting a new timeout");
    g_timeout_source = g_timeout_add_seconds(g_timeout_value, on_timeout_cb, NULL);
}

/* A configuration entity
 *
 * Each configuration has the working file and the default file.  It is
 * expected that the default configuration file remains unchanged while the
 * program is running, so the parsed file can be cached. But the working file
 * can be modified from various source, therefore we must parse the file upon
 * each request.
 */
typedef struct _configuration
{
    char *file_path;        ///< Path to the working file
    char *def_file_path;    ///< Path to the default file
    map_string_t *def;      ///< The default configuration
}
configuration_t;

typedef GVariant *(*configuration_getter_fn)(configuration_t *conf,
        const char *option, GError **error);

typedef bool      (*configuration_setter_fn)(configuration_t *conf,
        const char *option, GVariant *variant, GError **error);

static configuration_t *configuration_new(const char *file_path, const char *def_file_path)
{
    configuration_t *self = xmalloc(sizeof(*self));
    self->file_path = xstrdup(file_path);
    self->def_file_path = xstrdup(def_file_path);
    self->def = NULL;

    return self;
}

/* A helper function for equality comparison.
 *
 * Checks if the configuration files equals to the paths in array
 */
static int configuration_compare_paths(configuration_t *self, const char *paths[2])
{
    if (strcmp(self->file_path, paths[0]) != 0)
        return -1;
    if (strcmp(self->def_file_path, paths[1]) != 0)
        return 1;
    return 0;
}

static void configuration_free(configuration_t *self)
{
    if (self == NULL)
        return;

    free(self->file_path);
    self->file_path = NULL;

    free(self->def_file_path);
    self->def_file_path = NULL;

    free_map_string(self->def);
    self->def = NULL;

    free(self);
}

static map_string_t *configuration_load_file(const char *file_path,
        map_string_t *preloaded, bool fail_on_noent, GError **error)
{
    map_string_t *conf = preloaded;

    if (conf == NULL)
        conf = new_map_string();

    if (!load_conf_file(file_path, conf, /*skip w/o values*/false)
         && (errno != ENOENT || fail_on_noent))
    {
        if (conf != preloaded)
            free_map_string(conf);

        conf = NULL;

        g_set_error(error,
                ABRT_FILE_ACCESS_ERROR, ABRT_FILE_ACCESS_ERROR_CODE,
                "Could not load configuration from '%s'", file_path);
    }

    return conf;
}

static bool configuration_save_file(const char *file_path, map_string_t *conf, GError **error)
{
    const bool retval = save_conf_file(file_path, conf);

    if (!retval)
        g_set_error(error,
               ABRT_FILE_ACCESS_ERROR, ABRT_FILE_ACCESS_ERROR_CODE,
               "Could not save configuration file '%s'", file_path);

    return retval;
}

static map_string_t *configuration_get_default(configuration_t *self, GError **error)
{
    if (self->def == NULL)
        self->def = configuration_load_file(self->def_file_path,
                /*No preloaded configuration*/NULL,
                /*Fail on ENOENT*/true,
                error);

    return self->def;
}

/*
 * Default value
 */
static bool configuration_set_default(configuration_t *self, const char *option, GError **error)
{
    map_string_t *const def_conf = configuration_get_default(self, error);
    if (def_conf == NULL)
        return false;

    const char *const def_value = get_map_string_item_or_NULL(def_conf, option);

    map_string_t *const conf = configuration_load_file(self->file_path,
            /*No preloaded configuration*/NULL,
            /*Fail on ENOENT*/true,
            error);

    if (conf == NULL)
        /* If the configuration file does not exist, the default value is used */
        return errno == ENOENT;

    const char *const cur_value = get_map_string_item_or_NULL(conf, option);
    bool retval = true;

    /* Avoid saving the configuration if nothing has changed */
    if (!((def_value == NULL && cur_value == NULL)
          || (def_value != NULL && cur_value != NULL && strcmp(def_value, cur_value) == 0)))
    {
        if (def_value != NULL)
            replace_map_string_item(conf, xstrdup(option), xstrdup(def_value));
        else
            remove_map_string_item(conf, option);

        retval = configuration_save_file(self->file_path, conf, error);
    }

    free_map_string(conf);
    return retval;
}

/*
 * Setters
 */

/* Stores value of GVariant argument in the configuration under key 'option'.
 *
 * Converts the GVariant to the underlaying type via 'transform' function.
 */
static bool configuration_set_gvariant(configuration_t *self, const char *option, GVariant *value,
        void (*transform)(map_string_t *, const char *, GVariant *),
        GError **error)
{
    map_string_t *const conf = configuration_load_file(self->file_path,
            /*No preloaded configuration*/NULL,
            /*Fail on ENOENT*/false,
            error);

    if (conf == NULL)
        return false;

    transform(conf, option, value);

    const bool retval = configuration_save_file(self->file_path, conf, error);

    free_map_string(conf);
    return retval;
}

static void bool_from_gvariant(map_string_t *conf, const char *option, GVariant *value)
{
    const bool raw_value = g_variant_get_boolean(value);
    log_debug("Save boolean option '%s':%d", option, raw_value);
    set_map_string_item_from_bool(conf, option, raw_value);
}

static void int32_from_gvariant(map_string_t *conf, const char *option, GVariant *value)
{
    const int raw_value = g_variant_get_int32(value);
    log_debug("Save int32 option '%s':%d", option, raw_value);
    set_map_string_item_from_int(conf, option, raw_value);
}

static void string_from_gvariant(map_string_t *conf, const char *option, GVariant *value)
{
    const char *const raw_value = g_variant_get_string(value, /*length*/ NULL);
    log_debug("Save string option '%s':%s", option, raw_value);
    set_map_string_item_from_string(conf, option, raw_value);
}

static void string_vector_from_gvariant(map_string_t *conf, const char *option, GVariant *value)
{
    const gchar **const raw_value = g_variant_get_strv(value, /*lenght -> request NULL terminated vector*/ NULL);
    log_debug("Save string vector option '%s'", option);
    set_map_string_item_from_string_vector(conf, option, (string_vector_ptr_t)raw_value);
    g_free(raw_value);
}

static bool configuration_set_string(configuration_t *self, const char *option, GVariant *value, GError **error)
{
    return configuration_set_gvariant(self, option, value, string_from_gvariant, error);
}

static bool configuration_set_string_vector(configuration_t *self, const char *option, GVariant *value, GError **error)
{
    return configuration_set_gvariant(self, option, value, string_vector_from_gvariant, error);
}

static bool configuration_set_bool(configuration_t *self, const char *option, GVariant *value, GError **error)
{
    return configuration_set_gvariant(self, option, value, bool_from_gvariant, error);
}

static bool configuration_set_int(configuration_t *self, const char *option, GVariant *value, GError **error)
{
    return configuration_set_gvariant(self, option, value, int32_from_gvariant, error);
}

static configuration_setter_fn configuration_setter_factory(GVariantType *type)
{
    if (g_variant_type_equal(G_VARIANT_TYPE_BOOLEAN, type))
        return configuration_set_bool;
    else if (g_variant_type_equal(G_VARIANT_TYPE_INT32, type))
        return configuration_set_int;
    else if (g_variant_type_equal(G_VARIANT_TYPE_STRING, type))
        return configuration_set_string;
    else if (g_variant_type_equal(G_VARIANT_TYPE_STRING_ARRAY, type))
        return configuration_set_string_vector;

    return NULL;
}

/*
 * Getters
 */

/* Gets the configuration option's value as GVariant
 *
 * Converts the GVariant to the underlaying type via 'transform' function.
 */
static GVariant *configuration_get_gvariant(configuration_t *self, const char *option,
        GVariant *(*transform)(map_string_t *conf, const char *option),
        GError **error)
{
    map_string_t *const def_conf = configuration_get_default(self, error);
    if (def_conf == NULL)
        return false;

    /* BEGIN: clone_map_string() */
    map_string_t *conf = new_map_string();
    map_string_iter_t iter;
    init_map_string_iter(&iter, def_conf);
    const char *key=NULL;
    const char *value=NULL;
    while(next_map_string_iter(&iter, &key, &value))
        replace_map_string_item(conf, xstrdup(key), xstrdup(value));
    /* END:   clone_map_string() */

    GError *working_error = NULL;
    if (!configuration_load_file(self->file_path, conf, /*Fail on ENOENT*/true, &working_error))
    {
        log_debug("Error while loading working configuration: %s", working_error->message);
        g_error_free(working_error);
    }

    GVariant *const retval = transform(conf, option);
    free_map_string(conf);
    if (!retval)
    {
        g_set_error(error,
                    ABRT_FILE_FORMAT_ERROR, ABRT_FILE_FORMAT_ERROR_CODE,
                    "Option '%s' has invalid value in file '%s'", option, self->file_path);
        log_warning("Option '%s' has invalid value in file '%s'", option, self->file_path);
    }
    return retval;
}

static GVariant *int32_to_gvariant(map_string_t *conf, const char *key)
{
    int value = 0;
    if (!try_get_map_string_item_as_int(conf, key, &value))
        return NULL;

    return g_variant_new_int32(value);
}

static GVariant *bool_to_gvariant(map_string_t *conf, const char *key)
{
    int value = 0;
    if (!try_get_map_string_item_as_bool(conf, key, &value))
        return NULL;

    return g_variant_new_boolean(value);
}

static GVariant *string_to_gvariant(map_string_t *conf, const char *key)
{
    char *value = NULL;
    if (!try_get_map_string_item_as_string(conf, key, &value))
        return NULL;

    GVariant *retval = g_variant_new_string(value);
    free(value);
    return retval;
}

static GVariant *string_vector_to_gvariant(map_string_t *conf, const char *key)
{
    string_vector_ptr_t value = NULL;
    if (!try_get_map_string_item_as_string_vector(conf, key, &value))
        return NULL;

    GVariant *retval = g_variant_new_strv((const gchar *const *)value, /*NULL terminated*/-1);
    string_vector_free(value);
    return retval;
}

static GVariant *configuration_get_string(configuration_t *self, const char *option, GError **error)
{
    return configuration_get_gvariant(self, option, string_to_gvariant, error);
}

static GVariant *configuration_get_int32(configuration_t *self, const char *option, GError **error)
{
    return configuration_get_gvariant(self, option, int32_to_gvariant, error);
}

static GVariant *configuration_get_bool(configuration_t *self, const char *option, GError **error)
{
    return configuration_get_gvariant(self, option, bool_to_gvariant, error);
}

static GVariant *configuration_get_string_vector(configuration_t *self, const char *option, GError **error)
{
    return configuration_get_gvariant(self, option, string_vector_to_gvariant, error);
}

static configuration_getter_fn configuration_getter_factory(GVariantType *type)
{
    if (g_variant_type_equal(G_VARIANT_TYPE_BOOLEAN, type))
        return configuration_get_bool;
    else if (g_variant_type_equal(G_VARIANT_TYPE_INT32, type))
        return configuration_get_int32;
    else if (g_variant_type_equal(G_VARIANT_TYPE_STRING, type))
        return configuration_get_string;
    else if (g_variant_type_equal(G_VARIANT_TYPE_STRING_ARRAY, type))
        return configuration_get_string_vector;

    return NULL;
}

/* A single property entity
 *
 * This structure provides mapping between a D-Bus property and a configuration option.
 */
typedef struct _property
{
    char *name;                         ///< Property Name in the XML file and Option Name in configuration
    GVariantType *type;                 ///< GLib's type
    char *type_string;                  ///< GLib's type string
    configuration_t *conf;              ///< A configuration which contains this option (Not owned)
    configuration_getter_fn getter;     ///< Getter function
    configuration_setter_fn setter;     ///< Setter function
}
property_t;

static property_t *property_new(const char *name, GVariantType *type, configuration_t *conf,
        configuration_getter_fn getter, configuration_setter_fn setter)
{
    property_t *self = xmalloc(sizeof(*self));

    self->name = xstrdup(name);
    self->type = type;
    self->type_string = NULL;
    self->conf = conf;
    self->getter = getter;
    self->setter = setter;

    return self;
}

static void property_free(property_t *self)
{
    if (self == NULL)
        return;

    free(self->name);
    self->name = NULL;

    g_variant_type_free(self->type);
    self->type = NULL;

    g_free(self->type_string);
    self->type_string = NULL;
}

static const char *property_get_type_string(property_t *self)
{
    if (self->type_string == NULL)
        self->type_string = g_variant_type_dup_string(self->type);

    return self->type_string;
}

static bool property_set(property_t *self, GVariant *args, GError **error)
{
    if (self->setter)
        return self->setter(self->conf, self->name, args, error);

    g_set_error(error,
            ABRT_REFLECTION_UNSUPPORTED_TYPE_ERROR, ABRT_REFLECTION_UNSUPPORTED_TYPE_ERROR_CODE,
            "Type with signature '%s' is not supported", property_get_type_string(self));

    return false;
}

static GVariant *property_get(property_t *self, GError **error)
{
    if (self->getter)
    {
        GVariant *retval = self->getter(self->conf, self->name, error);
        assert((retval != NULL || *error != NULL) || !"GError must be set if bool option cannot be returned.");
        return retval;
    }

    g_set_error(error,
            ABRT_REFLECTION_UNSUPPORTED_TYPE_ERROR, ABRT_REFLECTION_UNSUPPORTED_TYPE_ERROR_CODE,
            "Type with signature '%s' is not supported", property_get_type_string(self));

    return NULL;
}

static bool property_reset(property_t *self, GError **error)
{
    return configuration_set_default(self->conf, self->name, error);
}

/* A single D-Bus node
 */
typedef struct _dbus_conf_node
{
    GDBusNodeInfo *node;        ///< Parsed XML file
    GSList *configurations;     ///< List of all configurations (configuration_t)
    GHashTable *properties;     ///< List of properties (property_t)
}
dbus_conf_node_t;

static dbus_conf_node_t *dbus_conf_node_new(GDBusNodeInfo *node, GSList *configurations, GHashTable *properties)
{
    if (node->path == NULL)
    {
        error_msg("Node misses 'name' attribute");
        return NULL;
    }

    if (node->interfaces[0] == NULL)
    {
        error_msg("Node has no interface defined");
        return NULL;
    }

    dbus_conf_node_t *self = xmalloc(sizeof(*self));

    self->node = node;
    self->configurations = configurations;
    self->properties = properties;

    return self;
}

static void dbus_conf_node_free(dbus_conf_node_t *self)
{
    if (self == NULL)
        return;

    g_dbus_node_info_unref(self->node);
    self->node = NULL;

    g_slist_free_full(self->configurations, (GDestroyNotify)configuration_free);
    self->configurations = NULL;

    g_hash_table_destroy(self->properties);
    self->properties = NULL;
}

static const char* dbus_conf_node_get_path(dbus_conf_node_t *self)
{
    return self->node->path;
}

static GDBusInterfaceInfo *dbus_conf_node_get_interface(dbus_conf_node_t *self)
{
    return self->node->interfaces[0];
}

static property_t *dbus_conf_node_get_property(dbus_conf_node_t *self, const char *property_name, GError **error)
{
    gpointer property = g_hash_table_lookup(self->properties, property_name);

    if (property == NULL)
        g_set_error(error,
            ABRT_REFLECTION_MISSING_MEMBER_ERROR, ABRT_REFLECTION_MISSING_MEMBER_ERROR_CODE,
            "Could find property '%s'", property_name);

    return (property_t *)property;
}

/* SetDefault D-Bus method handler
 */
static void dbus_conf_node_handle_configuration_method_call(GDBusConnection *connection,
                        const gchar *caller,
                        const gchar *object_path,
                        const gchar *interface_name,
                        const gchar *method_name,
                        GVariant    *parameters,
                        GDBusMethodInvocation *invocation,
                        gpointer    user_data)
{
    log_debug("Set Default Property");

    reset_timeout();

    if (polkit_check_authorization_dname(caller, "com.redhat.problems.configuration.update") != PolkitYes)
    {
        log_notice("not authorized");
        g_dbus_method_invocation_return_dbus_error(invocation,
                "com.redhat.problems.configuration.AuthFailure",
                _("Not Authorized"));
        return;
    }

    const char *property_name = NULL;
    g_variant_get(parameters, "(&s)", &property_name);

    log_debug("Going to reset value of '%s'", property_name);

    GError *error = NULL;

    property_t *prop = dbus_conf_node_get_property((dbus_conf_node_t *)user_data, property_name, &error);
    if (prop == NULL || !property_reset(prop, &error))
    {
        g_dbus_method_invocation_return_gerror(invocation, error);
        g_error_free(error);
        return;
    }

    g_dbus_method_invocation_return_value(invocation, NULL);
}

/* Get D-Bus property handler
 */
static GVariant *dbus_conf_node_handle_get_property(GDBusConnection *connection,
                        const gchar *caller,
                        const gchar *object_path,
                        const gchar *interface_name,
                        const gchar *property_name,
                        GError      **error,
                        gpointer    user_data)
{
    log_debug("Get Property '%s'", property_name);

    reset_timeout();

    property_t *prop = dbus_conf_node_get_property((dbus_conf_node_t *)user_data, property_name, error);
    /* Paranoia: this should never happen*/
    if (prop == NULL)
        return NULL;

    return property_get(prop, error);
}

/* Set D-Bus property handler
 */
static gboolean dbus_conf_node_handle_set_property(GDBusConnection *connection,
                        const gchar *caller,
                        const gchar *object_path,
                        const gchar *interface_name,
                        const gchar *property_name,
                        GVariant    *args,
                        GError      **error,
                        gpointer    user_data)
{
    log_debug("Set Property '%s'", property_name);

    reset_timeout();

    if (polkit_check_authorization_dname(caller, "com.redhat.problems.configuration.update") != PolkitYes)
    {
        log_notice("not authorized");

        g_set_error(error,
                ABRT_AUTHORIZATION_ERROR, ABRT_AUTHORIZATION_ERROR_CODE,
                _("Not Authorized"));

        return FALSE;
    }

    property_t *prop = dbus_conf_node_get_property((dbus_conf_node_t *)user_data, property_name, error);
    /* Paranoia: this should never happen*/
    if (prop == NULL)
        return false;

    return property_set(prop, args, error);
}

static GDBusInterfaceVTable *dbus_conf_node_get_vtable(dbus_conf_node_t *node)
{
    static GDBusInterfaceVTable default_vtable =
    {
        .method_call = NULL,
        .get_property = dbus_conf_node_handle_get_property,
        .set_property = dbus_conf_node_handle_set_property,
    };

    return &default_vtable;
}

static GDBusInterfaceVTable *dbus_conf_node_get_configuration_vtable(dbus_conf_node_t *node)
{
    static GDBusInterfaceVTable default_vtable =
    {
        .method_call = dbus_conf_node_handle_configuration_method_call,
        .get_property = NULL,
        .set_property = NULL,
    };

    return &default_vtable;
}

/* Helpers for Annotation parsing */
struct annot
{
    const char *name;
    const char *value;
};

static void parse_annotations(GDBusAnnotationInfo **annotations, struct annot annots[], size_t count)
{
    GDBusAnnotationInfo **an_iter = annotations;

    while(*an_iter != NULL)
    {
        for (size_t i = 0; i < count; ++i)
            if (strcmp(annots[i].name, (*an_iter)->key) == 0)
                annots[i].value = (*an_iter)->value;

        ++an_iter;
    }
}

/* Builds the internal representation of configuration node from a D-Bus XML interface file.
 */
static dbus_conf_node_t *dbus_conf_node_from_node(GDBusNodeInfo *node)
{
    if (node->interfaces[0] == NULL)
    {
        error_msg("Node has no interface defined");
        return NULL;
    }

    struct annot annots[] = {
        { .name=ANNOTATION_NAME_CONF_FILE,     .value=NULL },
        { .name=ANNOTATION_NAME_DEF_CONF_FILE, .value=NULL },
    };

    /* Parse the implicit file paths.
     * The implicit paths are stored as child annotation elements of
     * <interface> element. These paths are use when a property does not have
     * its own file paths.
     *
     * Both of them are required. This is simplification because  this rule does
     * not make sense if all of the properties have its own file paths.
     */
    parse_annotations(node->annotations, annots, sizeof(annots)/sizeof(annots[0]));

    bool misses_annot = false;
    for (size_t i = 0; i < ARRAY_SIZE(annots); ++i)
    {
        if (annots[i].value == NULL)
        {
            error_msg("Configuration node '%s' misses annotation '%s'", node->path, annots[i].name);
            misses_annot = true;
        }
    }

    if (misses_annot)
        return NULL;

    /* The following two variable can be omitted but the configuraion_new() call */
    /* would be less understandable. */
    const char *const conf_file = annots[0].value;
    const char *const def_conf_file = annots[1].value;
    configuration_t * conf = configuration_new(conf_file, def_conf_file);

    /* List of known configuration file paths pairs (file, default file) */
    GSList *configurations = g_slist_prepend(NULL, conf);

    GHashTable *properties = g_hash_table_new_full(g_str_hash, g_str_equal,
            (GDestroyNotify)NULL, (GDestroyNotify)property_free);

    for(GDBusPropertyInfo **prop_iter = node->interfaces[0]->properties; *prop_iter != NULL; ++prop_iter)
    {
        /* Use the node's default configuration file pair */
        configuration_t *prop_conf = conf;

        annots[0].value = NULL;
        annots[1].value = NULL;

        /* Check whether the current property has configuration paths annotations.
         * It must have either both or none!
         */
        parse_annotations((*prop_iter)->annotations, annots, ARRAY_SIZE(annots));

        if (annots[0].value != NULL && annots[1].value != NULL)
        {
            const char *paths[2] = { annots[0].value, annots[1].value };
            /* Try to find a configuration which equals to this configuration files pair */
            GSList *lst_item = g_slist_find_custom(configurations,
                    (gpointer)paths,
                    (GCompareFunc)configuration_compare_paths);

            /* If such configuration object does not exist yet, create a new one. */
            if (lst_item == NULL)
            {
                /* The following two variable can be omitted but the configuraion_new() */
                /* call would be less understandable. */
                const char *const prop_conf_file = annots[0].value;
                const char *const prop_def_conf_file = annots[1].value;
                prop_conf = configuration_new(prop_conf_file, prop_def_conf_file);

                configurations = g_slist_prepend(configurations, prop_conf);
            }
            else
                prop_conf = (configuration_t *)lst_item->data;
        }
        else if (annots[0].value == NULL && annots[1].value != NULL)
        {
            error_msg("Property '%s' misses annotation '%s'", (*prop_iter)->name, annots[0].name);
            continue;
        }
        else if (annots[0].value != NULL && annots[1].value == NULL)
        {
            error_msg("Property '%s' misses annotation '%s'", (*prop_iter)->name, annots[1].name);
            continue;
        }

        GVariantType *prop_type = g_variant_type_new((*prop_iter)->signature);
        configuration_getter_fn prop_get = configuration_getter_factory(prop_type);
        configuration_setter_fn prop_set = configuration_setter_factory(prop_type);

        /* We don't mind if the property's type is not supported. We still want
         * to provide the property, because hiding it would be more confusing.
         */
        if (prop_get == NULL)
            error_msg("Property '%s' has unsupported getter type", (*prop_iter)->name);
        if (prop_set == NULL)
            error_msg("Property '%s' has unsupported setter type", (*prop_iter)->name);

        property_t *prop = property_new((*prop_iter)->name, prop_type, prop_conf, prop_get, prop_set);
        g_hash_table_replace(properties, (*prop_iter)->name, prop);
    }

    return dbus_conf_node_new(node, configurations, properties);
}

static dbus_conf_node_t *dbus_conf_node_from_file(const char *iface_file_path)
{
    char *xmldata = xmalloc_open_read_close(iface_file_path, /*maxsize*/NULL);
    if (xmldata == NULL)
    {
        error_msg("Cannot create configuration node from file '%s'", iface_file_path);
        return NULL;
    }

    GError *error = NULL;
    GDBusNodeInfo *node = g_dbus_node_info_new_for_xml(xmldata, &error);

    if (error)
    {
        free(xmldata);
        error_msg("Could not parse interface file '%s': %s", iface_file_path, error->message);
        g_error_free(error);
        return NULL;
    }

    dbus_conf_node_t *conf_node = dbus_conf_node_from_node(node);

    /* Failed to create a configuration node, node is unchanged and must be released */
    if (conf_node == NULL)
        g_dbus_node_info_unref(node);

    free(xmldata);

    return conf_node;
}

/* Go through files within the directory and try to convert them to a D-Bus
 * configuration nodes.
 */
static GList *load_configurators(const char *file_dir)
{
    log_debug("Loading configuration XML interfaces from '%s'", file_dir);

    GList *conf_files = get_file_list(file_dir, "xml");
    GList *result = NULL;

    for (GList *iter = conf_files; iter != NULL; iter = g_list_next(iter))
    {
        file_obj_t *const file = (file_obj_t *)iter->data;

        const char *const filename = fo_get_filename(file);
        if (prefixcmp(filename, ABRT_CONF_IFACE_PFX) != 0)
            /* Skipping the current file because it is not Problems Configuration iface */
            continue;

        /* The non-default interfaces has a short string between ABRT_CONF_IFACE_PFX prefix */
        /* and ".xml" suffix (get_file_list() chops the sfx (.xml))*/
        if (filename[strlen(ABRT_CONF_IFACE_PFX)] == '\0')
            /* Skipping the default configuration iface */
            continue;

        const char *const fullpath = fo_get_fullpath(file);
        log_debug("Processing interface file '%s'", fullpath);

        dbus_conf_node_t *const conf_node = dbus_conf_node_from_file(fullpath);
        if (conf_node != NULL)
            result = g_list_prepend(result, conf_node);
    }

    free_file_list(conf_files);

    return result;
}

static void on_bus_acquired(GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
    log_debug("Going to register the configuration objects on bus '%s'", name);

    for (GList *iter = (GList *)user_data; iter != NULL; iter = g_list_next(iter))
    {
        dbus_conf_node_t *node = (dbus_conf_node_t *)iter->data;

        log_debug("Registering dbus object '%s'", dbus_conf_node_get_path(node));

        GError *error = NULL;
        /* Register the interface parsed from a XML file */
        guint registration_id = g_dbus_connection_register_object(connection,
                                        dbus_conf_node_get_path(node),
                                        dbus_conf_node_get_interface(node),
                                        dbus_conf_node_get_vtable(node),
                                        node,
                                        /*destroy notify*/NULL,
                                        &error);

        if (registration_id == 0)
        {
            error_msg("Could not register object '%s': %s", dbus_conf_node_get_path(node), error->message);
            g_error_free(error);
        }

        /* Register interface for SetDefault() method */
        registration_id = g_dbus_connection_register_object(connection,
                                        dbus_conf_node_get_path(node),
                                        g_default_node->interfaces[0],
                                        dbus_conf_node_get_configuration_vtable(node),
                                        node,
                                        /*destroy notify*/NULL,
                                        &error);

        if (registration_id == 0)
        {
            error_msg("Could not register object '%s': %s", dbus_conf_node_get_path(node), error->message);
            g_error_free(error);
        }
    }

    reset_timeout();
}

static void on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
    log_debug("Acquired the name '%s' on the system bus", name);
}

static void on_name_lost(GDBusConnection *connection,
                      const gchar *name,
                      gpointer user_data)
{
    log_warning(_("The name '%s' has been lost, please check if other "
              "service owning the name is not running.\n"), name);
    exit(1);
}

int main(int argc, char *argv[])
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif
    guint owner_id;

    glib_init();
    abrt_init(argv);

    const char *program_usage_string = _(
        "& [options]"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_t = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_INTEGER('t', NULL, &g_timeout_value, _("Exit after NUM seconds of inactivity")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    msg_prefix = "abrt-configuration"; /* for log_warning(), error_msg() and such */

    if (getuid() != 0)
        error_msg_and_die(_("This program must be run as root."));


    GError *error = NULL;
    g_default_node = g_dbus_node_info_new_for_xml(g_default_xml, &error);
    if (error != NULL)
        error_msg_and_die("Could not parse the default internface: %s", error->message);

    GList *conf_nodes = load_configurators(PROBLEMS_CONFIG_INTERFACES_DIR);
    if (conf_nodes == NULL)
        error_msg_and_die("No configuration interface file loaded. Exiting");

    owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
                              ABRT_CONF_DBUS_NAME,
                              G_BUS_NAME_OWNER_FLAGS_NONE,
                              on_bus_acquired,
                              on_name_acquired,
                              on_name_lost,
                              conf_nodes,
                              (GDestroyNotify)NULL);

    g_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(g_loop);

    log_notice("Cleaning up");

    g_bus_unown_name(owner_id);

    g_list_free_full(conf_nodes, (GDestroyNotify)dbus_conf_node_free);

    g_dbus_node_info_unref(g_default_node);

    free_abrt_conf_data();

    return 0;
}
