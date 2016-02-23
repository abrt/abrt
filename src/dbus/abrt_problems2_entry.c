/*
  Copyright (C) 2015  ABRT team

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "libabrt.h"
#include "abrt_problems2_entry.h"

#include <dbus/dbus.h>
#include <gio/gunixfdlist.h>

typedef struct
{
    char *p2e_dirname;
    AbrtP2EntryState p2e_state;
} AbrtP2EntryPrivate;

struct _AbrtP2Entry
{
    GObject parent_instance;
    AbrtP2EntryPrivate *pv;
};

G_DEFINE_TYPE_WITH_PRIVATE(AbrtP2Entry, abrt_p2_entry, G_TYPE_OBJECT)

static void abrt_p2_entry_finalize(GObject *gobject)
{
    AbrtP2EntryPrivate *pv = abrt_p2_entry_get_instance_private(ABRT_P2_ENTRY(gobject));
    free(pv->p2e_dirname);
}

static void abrt_p2_entry_class_init(AbrtP2EntryClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = abrt_p2_entry_finalize;
}

static void abrt_p2_entry_init(AbrtP2Entry *self)
{
    self->pv = abrt_p2_entry_get_instance_private(self);
}

AbrtP2Entry *abrt_p2_entry_new(char *dirname)
{
    return abrt_p2_entry_new_with_state(dirname, ABRT_P2_ENTRY_STATE_COMPLETE);
}

AbrtP2Entry *abrt_p2_entry_new_with_state(char *dirname,
            AbrtP2EntryState state)
{
    AbrtP2Entry *entry = g_object_new(TYPE_ABRT_P2_ENTRY, NULL);
    entry->pv->p2e_dirname = dirname;
    entry->pv->p2e_state = state;

    return entry;
}

AbrtP2EntryState abrt_p2_entry_state(AbrtP2Entry *entry)
{
    return entry->pv->p2e_state;
}

void abrt_p2_entry_set_state(AbrtP2Entry *entry, AbrtP2EntryState state)
{
    entry->pv->p2e_state = state;
}

const char *abrt_p2_entry_problem_id(AbrtP2Entry *entry)
{
    return entry->pv->p2e_dirname;
}

int abrt_p2_entry_accessible_by_uid(AbrtP2Entry *entry,
            uid_t uid,
            struct dump_dir **dd)
{
    struct dump_dir *tmp = dd_opendir(entry->pv->p2e_dirname, DD_OPEN_FD_ONLY
                                                              | DD_FAIL_QUIETLY_ENOENT
                                                              | DD_FAIL_QUIETLY_EACCES);
    if (tmp == NULL)
    {
        VERB2 perror_msg("can't open problem directory '%s'",
                         entry->pv->p2e_dirname);

        return -ENOTDIR;
    }

    const int ret = dd_accessible_by_uid(tmp, uid) ? 0 : -EACCES;

    if (ret == 0 && dd != NULL)
        *dd = tmp;
    else
        dd_close(tmp);

    return ret;
}

int abrt_p2_entry_delete(AbrtP2Entry *entry, uid_t caller_uid, GError **error)
{
    struct dump_dir *dd = NULL;
    int ret = abrt_p2_entry_accessible_by_uid(entry, caller_uid, &dd);
    if (ret != 0)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED,
                    "You are not authorized to delete the problem");

        return ret;
    }

    if (entry->pv->p2e_state == ABRT_P2_ENTRY_STATE_DELETED)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Problem entry is already deleted");

        return -EINVAL;
    }

    dd = dd_fdopendir(dd, DD_DONT_WAIT_FOR_LOCK);
    if (dd == NULL)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Cannot lock the problem. Check system logs.");

        return -EWOULDBLOCK;
    }

    ret = dd_delete(dd);
    if (ret != 0)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Failed to remove problem data. Check system logs.");

        dd_close(dd);
        return ret;
    }

    abrt_p2_entry_set_state(entry, ABRT_P2_ENTRY_STATE_DELETED);

    return ret;
}

GVariant *abrt_p2_entry_problem_data(AbrtP2Entry *node,
            uid_t caller_uid,
            GError **error)
{
    struct dump_dir *dd = abrt_p2_entry_open_dump_dir(node,
                                                      caller_uid,
                                                      DD_OPEN_READONLY,
                                                      error);
    if (dd == NULL)
        return NULL;

    problem_data_t *pd = create_problem_data_from_dump_dir(dd);
    problem_data_add_text_noteditable(pd, CD_DUMPDIR, node->pv->p2e_dirname);

    GVariantBuilder response_builder;
    g_variant_builder_init(&response_builder, G_VARIANT_TYPE_ARRAY);

    GHashTableIter pd_iter;
    char *element_name;
    struct problem_item *element_info;
    g_hash_table_iter_init(&pd_iter, pd);
    while (g_hash_table_iter_next(&pd_iter, (void**)&element_name, (void**)&element_info))
    {
        unsigned long size = 0;
        if (problem_item_get_size(element_info, &size) != 0)
        {
            log_notice("Can't get stat of : '%s'", element_info->content);
            continue;
        }

        g_variant_builder_add(&response_builder, "{s(its)}",
                                                 element_name,
                                                 element_info->flags,
                                                 size,
                                                 element_info->content);
    }

    problem_data_free(pd);
    dd_close(dd);

    return g_variant_new("(a{s(its)})", &response_builder);
}

struct dump_dir *abrt_p2_entry_open_dump_dir(AbrtP2Entry *entry,
             uid_t caller_uid,
             int dd_flags,
             GError **error)
{
    struct dump_dir *dd = NULL;
    if (0 != abrt_p2_entry_accessible_by_uid(entry, caller_uid, &dd))
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED,
                    "You are not authorized to access the problem");

        return NULL;
    }

    dd = dd_fdopendir(dd, dd_flags);
    if (dd == NULL)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_IO_ERROR,
                    "Failed reopen dump directory");

        return NULL;
    }

    return dd;
}

/**
 * Read elements
 */
GVariant *abrt_p2_entry_read_elements(AbrtP2Entry *entry,
             gint32 flags,
             GVariant *elements,
             GUnixFDList *fd_list,
             uid_t caller_uid,
             long max_size,
             long max_unix_fds,
             GError **error)
{
    if ((flags & ABRT_P2_ENTRY_READ_ALL_FD) && (flags & ABRT_P2_ENTRY_READ_ALL_NO_FD))
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                    "Invalid arguments 'ALL FD' ~ 'ALL NO FD'");

        return NULL;
    }

    struct dump_dir *dd = abrt_p2_entry_open_dump_dir(entry,
                                                      caller_uid,
                                                      DD_OPEN_READONLY | DD_DONT_WAIT_FOR_LOCK,
                                                      error);
    if (dd == NULL)
        return NULL;

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));

    size_t loaded_size = 0;
    gchar *name = NULL;
    GVariantIter iter;
    g_variant_iter_init(&iter, elements);
    /* No need to free 'name' unless breaking out of the loop */
    while (g_variant_iter_loop(&iter, "s", &name))
    {
        log_debug("Reading element: %s", name);
        /* Do ask me why -> see libreport xmalloc_read() */
        size_t data_size = (INT_MAX - 4095);

        int elem_type = 0;
        char *data = NULL;
        int fd = -1;
        const int r = problem_data_load_dump_dir_element(dd,
                                                         name,
                                                         &data,
                                                         &elem_type,
                                                         &fd);
        if (r < 0)
        {
            if (r == -ENOENT)
                log_debug("Element does not exist: %s", name);
            else if (r == -EINVAL)
                error_msg("Attempt to read prohibited data: '%s'", name);
            else
                error_msg("Failed to open %s: %s", name, strerror(-r));

            continue;
        }

        if (   ((flags & ABRT_P2_ENTRY_READ_ONLY_TEXT)     && !(elem_type & CD_FLAG_TXT))
            || ((flags & ABRT_P2_ENTRY_READ_ONLY_BIG_TEXT) && !(elem_type & CD_FLAG_BIGTXT))
            || ((flags & ABRT_P2_ENTRY_READ_ONLY_BINARY)   && !(elem_type & CD_FLAG_BIN))
           )
        {
            log_debug("Element is not of the requested type: %s", name);

            free(data);
            close(fd);
            continue;
        }

        if ((flags & ABRT_P2_ENTRY_READ_ALL_FD) || !(elem_type & CD_FLAG_TXT))
        {
            log_debug("Rewinding file descriptor %d", fd);

            free(data);
            if (lseek(fd, 0, SEEK_SET))
            {
                perror_msg("Failed to rewind file descriptor of %s", name);

                close(fd);
                continue;
            }
        }

        if (   (flags & ABRT_P2_ENTRY_READ_ALL_FD)
            || (!(flags & ABRT_P2_ENTRY_READ_ALL_NO_FD) && !(elem_type & CD_FLAG_TXT)))
        {
            if (g_unix_fd_list_get_length(fd_list) == max_unix_fds)
            {
                error_msg("Reached limit of UNIX FDs per message: %ld", max_unix_fds);
                continue;
            }

            GError *error = NULL;
            const gint pos = g_unix_fd_list_append(fd_list, fd, &error);
            close(fd);
            if (error != NULL)
            {
                error_msg("Failed to add file descriptor of %s: %s",
                          name,
                          error->message);

                g_error_free(error);
                continue;
            }

            log_debug("Adding new Unix FD at position: %d",  pos);

            g_variant_builder_add(&builder, "{sv}",
                                            name,
                                            g_variant_new("h",
                                                          pos));
            continue;
        }

        if (!(elem_type & CD_FLAG_TXT))
        {
            data = xmalloc_read(fd, &data_size);

            log_debug("Re-loaded entire element: %zu Bytes", data_size);
        }
        else
            data_size = strlen(data);

        close(fd);

        if (data_size > DBUS_MAXIMUM_ARRAY_LENGTH)
        {
            error_msg("Element '%s' cannot be returned as array due to length limit: %ld",
                      name,
                      (long)DBUS_MAXIMUM_ARRAY_LENGTH);

            free(data);

            continue;
        }

        if (data_size > max_size || loaded_size > max_size - data_size)
        {
            error_msg("With element '%s', reached runtime data size limit: %ld",
                      name,
                      max_size);

            free(data);

            continue;
        }

        if (loaded_size > ULONG_MAX - data_size)
        {
            error_msg("With element '%s', reached static data size limit: %ld",
                      name,
                      max_size);

            free(data);

            continue;
        }

        loaded_size += data_size;

        if (elem_type & CD_FLAG_BIN)
        {
            log_debug("Adding element binary data");
            g_variant_builder_add(&builder, "{sv}",
                                             name,
                                             g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                                                       data,
                                                                       data_size,
                                                                       sizeof(char)));
        }
        else
        {
            log_debug("Adding element text data");
            g_variant_builder_add(&builder, "{sv}",
                                            name,
                                            g_variant_new_string(data));
        }

        free(data);
    }

    dd_close(dd);

    GVariant *retval_body[1];
    retval_body[0] = g_variant_builder_end(&builder);
    return  g_variant_new_tuple(retval_body, ARRAY_SIZE(retval_body));
}

/**
 * Asynchronous version of Read elements
 */
typedef struct
{
    gint32 flags;
    GVariant *elements;
    GUnixFDList *fd_list;
    uid_t caller_uid;
    long  max_size;
    long  max_unix_fds;
} AbrtP2EntryReadElementsData;

#define abrt_p2_entry_read_elements_data_new() \
    xmalloc(sizeof(AbrtP2EntryReadElementsData))

static inline void abrt_p2_entry_read_elements_data_free(AbrtP2EntryReadElementsData *data)
{
    free(data);
}

void abrt_p2_entry_read_elements_async_task(GTask *task,
            gpointer source_object,
            gpointer task_data,
            GCancellable *cancellable)
{
    AbrtP2Entry *entry = source_object;
    AbrtP2EntryReadElementsData *data = task_data;

    GError *error = NULL;
    GVariant *response = abrt_p2_entry_read_elements(entry,
                                                     data->flags,
                                                     data->elements,
                                                     data->fd_list,
                                                     data->caller_uid,
                                                     data->max_size,
                                                     data->max_unix_fds,
                                                     &error);

    if (error == NULL)
        g_task_return_pointer(task, response, (GDestroyNotify)g_variant_unref);
    else
        g_task_return_error(task, error);
}

void abrt_p2_entry_read_elements_async(AbrtP2Entry *entry,
            gint32 flags,
            GVariant *elements,
            GUnixFDList *fd_list,
            uid_t caller_uid,
            long max_size,
            long max_unix_fds,
            GCancellable *cancellable,
            GAsyncReadyCallback callback,
            gpointer user_data)
{
    AbrtP2EntryReadElementsData *data = abrt_p2_entry_read_elements_data_new();
    data->flags = flags;
    data->elements = elements;
    data->fd_list = fd_list;
    data->caller_uid = caller_uid;
    data->max_size = max_size;
    data->max_unix_fds = max_unix_fds;

    GTask *task = g_task_new(entry, cancellable, callback, user_data);
    g_task_set_task_data(task, data, (GDestroyNotify)abrt_p2_entry_read_elements_data_free);
    g_task_run_in_thread(task, abrt_p2_entry_read_elements_async_task);
    g_object_unref(task);
}

GVariant *abrt_p2_entry_read_elements_finish(AbrtP2Entry *entry,
           GAsyncResult *result,
           GError **error)
{
    g_return_val_if_fail(g_task_is_valid(result, entry), NULL);

    return g_task_propagate_pointer(G_TASK(result), error);
}

/**
 * Save elements
 */
GVariant *abrt_p2_entry_save_elements(AbrtP2Entry *entry,
            gint32 flags,
            GVariant *elements,
            GUnixFDList *fd_list,
            uid_t caller_uid,
            AbrtP2EntrySaveElementsLimits *limits,
            GError **error)
{
    struct dump_dir *dd = abrt_p2_entry_open_dump_dir(entry,
                                                      caller_uid,
                                                      DD_DONT_WAIT_FOR_LOCK,
                                                      error);
    if (dd == NULL)
        return NULL;

    abrt_p2_entry_save_elements_in_dump_dir(dd,
                                            flags,
                                            elements,
                                            fd_list,
                                            caller_uid,
                                            limits,
                                            error);

    return NULL;
}

/**
 * Save elements in a dump directory
 */
int abrt_p2_entry_save_elements_in_dump_dir(struct dump_dir *dd,
            gint32 flags,
            GVariant *elements,
            GUnixFDList *fd_list,
            uid_t caller_uid,
            AbrtP2EntrySaveElementsLimits *limits,
            GError **error)
{
    int retval = 0;

    gchar *name = NULL;
    GVariant *value = NULL;
    GVariantIter iter;
    g_variant_iter_init(&iter, elements);

    off_t dd_size = dd_compute_size(dd, /*no flags*/0);
    if (dd_size < 0)
    {
        error_msg("Failed to get file system size of dump dir : %s",
                  strerror(-(int)dd_size));

        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_IO_ERROR,
                    "Dump directory file system size");

        return dd_size;
    }

    int dd_items = dd_get_items_count(dd);
    if (dd_items < 0)
    {
        error_msg("Failed to get count of dump dir elements: %s",
                  strerror(-dd_items));

        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_IO_ERROR,
                    "Dump directory elements count");

        return dd_items;
    }

    /* No need to free 'name' and 'container' unless breaking out of the loop */
    while (g_variant_iter_loop(&iter, "{sv}", &name, &value))
    {
        log_debug("Saving element: %s", name);

        struct stat item_stat;
        memset(&item_stat, 0, sizeof(item_stat));

        const int r = dd_item_stat(dd, name, &item_stat);
        if (r == -EINVAL)
        {
            error_msg("Attempt to save prohibited data: '%s'", name);

            g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED,
                        "Not allowed problem element name");

            retval = -EACCES;
            goto exit_loop_on_error;
        }
        else if (r == -ENOENT)
        {
            if (limits->elements_count != 0 && dd_items >= limits->elements_count)
            {
                error_msg("Cannot create new element '%s': reached the limit for elements %u",
                          name,
                          limits->elements_count);

                if (flags & ABRT_P2_ENTRY_ELEMENTS_COUNT_LIMIT_FATAL)
                    goto exit_loop_on_too_many_elements;

                continue;
            }

            ++dd_items;
        }
        else if (r < 0)
        {
            error_msg("Failed to get size of element '%s'", name);

            if (flags & ABRT_P2_ENTRY_IO_ERROR_FATAL)
            {
                g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_IO_ERROR,
                            "Failed to get size of underlying data");

                retval = r;
                goto exit_loop_on_error;
            }

            continue;
        }

        const off_t base_size = dd_size - item_stat.st_size;

        if (   g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)
            || g_variant_is_of_type(value, G_VARIANT_TYPE_BYTESTRING))
        {
            off_t data_size = 0;
            const char *data = NULL;
            if (g_variant_is_of_type(value, G_VARIANT_TYPE_BYTESTRING))
            {
                log_debug("Saving binary element");

                /* Using G_VARIANT_TYPE_BYTESTRING only to check the type. */
                gsize n_elements = 0;
                const gsize element_size = sizeof(guchar);
                data = g_variant_get_fixed_array(value,
                                                 &n_elements,
                                                 element_size);

                data_size = n_elements * element_size;
            }
            else
            {
                log_debug("Saving text element");

                gsize size = 0;
                data = g_variant_get_string(value, &size);
                if (size >= (1ULL << (8 * sizeof(off_t) - 1)))
                {
                    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_IO_ERROR,
                                "Cannot read huge text data");

                    retval = -EINVAL;
                    goto exit_loop_on_error;
                }

                data_size = (off_t)size;
            }

            if (allowed_new_user_problem_entry(caller_uid, name, data) == false)
            {
                error_msg("Not allowed for user %lu: %s = %s",
                          (long unsigned)caller_uid,
                          name,
                          data);

                g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                            "You are not allowed to create element '%s' containing '%s'",
                            name, data);

                retval = -EPERM;
                goto exit_loop_on_error;
            }

            /* Do not allow dump dir growing in case it already consumes
             * more than the limit */
            if (   limits->data_size != 0
                && data_size > item_stat.st_size
                && base_size + data_size > limits->data_size)
            {
                error_msg("Cannot save text element: "
                          "problem data size limit %lld, "
                          "data size %lld, "
                          "item size %lld, "
                          "base size %lld",
                          (long long int)limits->data_size,
                          (long long int)data_size,
                          (long long int)item_stat.st_size,
                          (long long int)base_size);

                if (flags & ABRT_P2_ENTRY_DATA_SIZE_LIMIT_FATAL)
                    goto exit_loop_on_too_big_data;

                continue;
            }

            dd_save_binary(dd, name, data, data_size);
            dd_size = base_size + data_size;
        }
        else if (g_variant_is_of_type(value, G_VARIANT_TYPE_HANDLE))
        {
            log_debug("Saving data from file descriptor");

            if (problem_entry_is_post_create_condition(name))
            {
                error_msg("post-create element as file descriptor: %s", name);

                g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                            "Element '%s' must be of '%s' D-Bus type",
                            name,
                            g_variant_type_peek_string(G_VARIANT_TYPE_STRING));

                retval = -EINVAL;
                goto exit_loop_on_error;
            }

            gint32 handle = g_variant_get_handle(value);

            int fd = g_unix_fd_list_get(fd_list, handle, error);
            if (*error != NULL)
            {
                error_msg("Failed to get file descriptor of %s: %s",
                          name,
                          (*error)->message);

                if (flags & ABRT_P2_ENTRY_IO_ERROR_FATAL)
                {
                    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_IO_ERROR,
                                "Failed to get passed file descriptor");

                    retval = -EIO;
                    goto exit_loop_on_error;
                }

                continue;
            }

            /* Do not allow dump dir growing */
            const off_t max_size = base_size > limits->data_size
                                    ? item_stat.st_size
                                    : limits->data_size - base_size;

            /* Make the file descriptor non-blocking. We will not wait for
             * data. An attacker could use it to stop the service from
             * function. */
            if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
            {
                perror_msg("Failed to set file descriptor of the '%s' item non-blocking:",
                           name);

                close(fd);
                if (flags & ABRT_P2_ENTRY_IO_ERROR_FATAL)
                {
                    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_IO_ERROR,
                                "Failed to set file file descriptor of the '%s' item non-blocking",
                                name);

                    retval = -EIO;
                    goto exit_loop_on_error;
                }

                continue;
            }

            const off_t r = dd_copy_fd(dd, name, fd, /*copy_flags*/0, max_size);
            close(fd);

            if (r < 0)
            {
                error_msg("Failed to save file descriptor");

                if (flags & ABRT_P2_ENTRY_IO_ERROR_FATAL)
                {
                    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_IO_ERROR,
                                "Failed to save data of passed file descriptor");

                    retval = r;
                    goto exit_loop_on_error;
                }

                continue;
            }

            if (r >= max_size)
            {
                error_msg("File descriptor was truncated due to size limit");

                if (flags & ABRT_P2_ENTRY_DATA_SIZE_LIMIT_FATAL)
                    goto exit_loop_on_too_big_data;

                /* the file has been created and its size is 'max_size' */
                dd_size = base_size + max_size;
            }
            else
                dd_size = base_size + r ;
        }
        else
        {
            error_msg("Unsupported type: %s", g_variant_get_type_string(value));

            if (flags & ABRT_P2_ENTRY_IO_ERROR_FATAL)
            {
                g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED,
                            "Not supported D-Bus type");

                retval = -ENOTSUP;
                goto exit_loop_on_error;
            }
        }
    }

    return 0;

exit_loop_on_too_big_data:
    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_LIMITS_EXCEEDED,
                "Problem data is too big");
    return -EFBIG;

exit_loop_on_too_many_elements:
    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_LIMITS_EXCEEDED,
                "Too many elements");
    return -E2BIG;

exit_loop_on_error:
    return retval;
}

/**
 * Asynchronous version of Save elements
 */
typedef struct {
    gint32 flags;
    GVariant *elements;
    GUnixFDList *fd_list;
    uid_t caller_uid;
    AbrtP2EntrySaveElementsLimits limits;
} AbrtP2EntrySaveElementsData;

#define abrt_p2_entry_save_elements_data_new() \
    xmalloc(sizeof(AbrtP2EntrySaveElementsData))

static inline void abrt_p2_entry_save_elements_data_free(AbrtP2EntrySaveElementsData *data)
{
    free(data);
}

static void abrt_p2_entry_save_elements_async_task(GTask *task,
            gpointer source_object, gpointer task_data,
            GCancellable *cancellable)
{
    AbrtP2Entry *entry = source_object;
    AbrtP2EntrySaveElementsData *data = task_data;

    GError *error = NULL;
    GVariant *response = abrt_p2_entry_save_elements(entry,
                                                     data->flags,
                                                     data->elements,
                                                     data->fd_list,
                                                     data->caller_uid,
                                                     &(data->limits),
                                                     &error);

    if (error == NULL)
        g_task_return_pointer(task, response, (GDestroyNotify)g_variant_unref);
    else
        g_task_return_error(task, error);
}

void abrt_p2_entry_save_elements_async(AbrtP2Entry *entry,
            gint32 flags,
            GVariant *elements,
            GUnixFDList *fd_list,
            uid_t caller_uid,
            AbrtP2EntrySaveElementsLimits *limits,
            GCancellable *cancellable,
            GAsyncReadyCallback callback,
            gpointer user_data)
{
    AbrtP2EntrySaveElementsData *data = abrt_p2_entry_save_elements_data_new();
    data->flags = flags;
    data->elements = elements;
    data->fd_list = fd_list;
    data->caller_uid = caller_uid;
    data->limits = *limits;

    GTask *task = g_task_new(entry, cancellable, callback, user_data);
    g_task_set_task_data(task, data, (GDestroyNotify)abrt_p2_entry_save_elements_data_free);
    g_task_run_in_thread(task,  abrt_p2_entry_save_elements_async_task);
    g_object_unref(task);
    return;
}

GVariant *abrt_p2_entry_save_elements_finish(AbrtP2Entry *entry,
            GAsyncResult *result,
            GError **error)
{
    g_return_val_if_fail(g_task_is_valid(result, entry), NULL);

    return g_task_propagate_pointer(G_TASK(result), error);
}


/**
 * Delete elements
 */
GVariant *abrt_p2_entry_delete_elements(AbrtP2Entry *entry,
            uid_t caller_uid,
            GVariant *elements,
            GError **error)
{
    struct dump_dir *dd = abrt_p2_entry_open_dump_dir(entry,
                                                      caller_uid,
                                                      DD_DONT_WAIT_FOR_LOCK,
                                                      error);
    if (dd == NULL)
        return NULL;

    gchar *name = NULL;
    GVariantIter iter;
    g_variant_iter_init(&iter, elements);

    /* No need to free 'name' unless breaking out of the loop */
    while (g_variant_iter_loop(&iter, "s", &name))
    {
        log_debug("Deleting element: %s", name);
        const int r = dd_delete_item(dd, name);

        if (r == -EINVAL)
            error_msg("Attempt to remove prohibited data: '%s'", name);
    }

    dd_close(dd);

    return NULL;
}

/*
 * Properties
 */
uid_t abrt_p2_entry_get_owner(AbrtP2Entry *entry,
            GError **error)
{
    struct dump_dir *dd = dd_opendir(entry->pv->p2e_dirname, DD_OPEN_FD_ONLY);
    if (dd == NULL)
    {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_IO_ERROR,
                    "Failed open dump directory");
        return -1;
    }

    const uid_t uid = dd_get_owner(dd);
    dd_close(dd);

    return uid;
}
