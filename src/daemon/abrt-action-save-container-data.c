/*
    Copyright (C) 2015  ABRT Team
    Copyright (C) 2015  RedHat inc.

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
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "libabrt.h"
#include <json.h>

void dump_docker_info(struct dump_dir *dd, const char *root_dir)
{
    if (!dd_exist(dd, FILENAME_CONTAINER))
        dd_save_text(dd, FILENAME_CONTAINER, "docker");

    json_object *json = NULL;
    char *mntnf_path = concat_path_file(dd->dd_dirname, FILENAME_MOUNTINFO);
    FILE *mntnf_file = fopen(mntnf_path, "r");
    free(mntnf_path);

    struct mount_point {
        const char *name;
        enum mountinfo_fields {
            MOUNTINFO_ROOT,
            MOUNTINFO_SOURCE,
        } field;
    } mount_points[] = {
        { "/sys/fs/cgroup/memory", MOUNTINFO_ROOT },
        { "/",                     MOUNTINFO_SOURCE },
    };

    char *container_id = NULL;
    char *output = NULL;

    /* initialized to 0 because we call mountinfo_destroy below */
    struct mountinfo mntnf = {0};

    for (size_t i = 0; i < ARRAY_SIZE(mount_points); ++i)
    {
        log_debug("Parsing container ID from mount point '%s'", mount_points[i].name);

        rewind(mntnf_file);

        /* get_mountinfo_for_mount_point() re-initializes &mntnf */
        mountinfo_destroy(&mntnf);
        int r = get_mountinfo_for_mount_point(mntnf_file, &mntnf, mount_points[i].name);

        if (r != 0)
        {
            log_debug("Mount poin not found");
            continue;
        }

        const char *mnt_info = NULL;
        switch(mount_points[i].field)
        {
            case MOUNTINFO_ROOT:
                mnt_info = MOUNTINFO_ROOT(mntnf);
                break;
            case MOUNTINFO_SOURCE:
                mnt_info = MOUNTINFO_MOUNT_SOURCE(mntnf);
                break;
            default:
                error_msg("BUG: forgotten MOUNTINFO field type");
                abort();
        }
        const char *last = strrchr(mnt_info, '/');
        if (last == NULL || strncmp("/docker-", last, strlen("/docker-")) != 0)
        {
            log_debug("Mounted source is not a docker mount source: '%s'", mnt_info);
            continue;
        }

        last = strrchr(last, '-');
        if (last == NULL)
        {
            log_debug("The docker mount point has unknown format");
            continue;
        }

        ++last;

        /* Why we copy only 12 bytes here?
         * Because only the first 12 characters are used by docker as ID of the
         * container. */
        container_id = xstrndup(last, 12);
        if (strlen(container_id) != 12)
        {
            log_debug("Failed to get container ID");
            continue;
        }

        char *docker_inspect_cmdline = NULL;
        if (root_dir != NULL)
            docker_inspect_cmdline = xasprintf("chroot %s /bin/sh -c \"docker inspect %s\"", root_dir, container_id);
        else
            docker_inspect_cmdline = xasprintf("docker inspect %s", container_id);

        log_debug("Executing: '%s'", docker_inspect_cmdline);
        output = run_in_shell_and_save_output(0, docker_inspect_cmdline, "/", NULL);

        free(docker_inspect_cmdline);

        if (output == NULL || strcmp(output, "[]\n") == 0)
        {
            log_debug("Unsupported container ID: '%s'", container_id);

            free(container_id);
            container_id = NULL;

            free(output);
            output = NULL;

            continue;
        }

        break;
    }
    fclose(mntnf_file);

    if (container_id == NULL)
    {
        error_msg("Could not inspect the container");
        goto dump_docker_info_cleanup;
    }

    dd_save_text(dd, FILENAME_CONTAINER_ID, container_id);
    dd_save_text(dd, FILENAME_DOCKER_INSPECT, output);

    json = json_tokener_parse(output);
    free(output);

    if (is_error(json))
    {
        error_msg("Unable parse response from docker");
        goto dump_docker_info_cleanup;
    }

    json_object *container = json_object_array_get_idx(json, 0);
    if (container == NULL)
    {
        error_msg("docker does not contain array of containers");
        goto dump_docker_info_cleanup;
    }

    json_object *config = NULL;
    if (!json_object_object_get_ex(container, "Config", &config))
    {
        error_msg("container does not have 'Config' member");
        goto dump_docker_info_cleanup;
    }

    json_object *image = NULL;
    if (!json_object_object_get_ex(config, "Image", &image))
    {
        error_msg("Config does not have 'Image' member");
        goto dump_docker_info_cleanup;
    }

    char *name = strtrimch(xstrdup(json_object_to_json_string(image)), '"');
    dd_save_text(dd, FILENAME_CONTAINER_IMAGE, name);
    free(name);

dump_docker_info_cleanup:
    if (json != NULL)
        json_object_put(json);

    mountinfo_destroy(&mntnf);

    return;
}

void dump_lxc_info(struct dump_dir *dd, const char *lxc_cmd)
{
    if (!dd_exist(dd, FILENAME_CONTAINER))
        dd_save_text(dd, FILENAME_CONTAINER, "lxc");

    char *mntnf_path = concat_path_file(dd->dd_dirname, FILENAME_MOUNTINFO);
    FILE *mntnf_file = fopen(mntnf_path, "r");
    free(mntnf_path);

    struct mountinfo mntnf;
    int r = get_mountinfo_for_mount_point(mntnf_file, &mntnf, "/");
    fclose(mntnf_file);

    if (r != 0)
    {
        error_msg("lxc processes must have re-mounted root");
        goto dump_lxc_info_cleanup;
    }

    const char *mnt_root = MOUNTINFO_ROOT(mntnf);
    const char *last_slash = strrchr(mnt_root, '/');
    if (last_slash == NULL || (strcmp("rootfs", last_slash +1) != 0))
    {
        error_msg("Invalid root path '%s'", mnt_root);
        goto dump_lxc_info_cleanup;
    }

    if (last_slash == mnt_root)
    {
        error_msg("root path misses container id: '%s'", mnt_root);
        goto dump_lxc_info_cleanup;
    }

    const char *tmp = strrchr(last_slash - 1, '/');
    if (tmp == NULL)
    {
        error_msg("root path misses first /: '%s'", mnt_root);
        goto dump_lxc_info_cleanup;
    }

    char *container_id = xstrndup(tmp + 1, (last_slash - tmp) - 1);

    dd_save_text(dd, FILENAME_CONTAINER_ID, container_id);
    dd_save_text(dd, FILENAME_CONTAINER_UUID, container_id);

    free(container_id);

    /* TODO: make a copy of 'config' */
    /* get mount point for MOUNTINFO_MOUNT_SOURCE(mntnf) + MOUNTINFO_ROOT(mntnf) */

dump_lxc_info_cleanup:
    mountinfo_destroy(&mntnf);
}

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    const char *dump_dir_name = ".";
    const char *root_dir = NULL;

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [-v] -d DIR\n"
        "\n"
        "Save container metadata"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR"     , _("Problem directory")),
        OPT_STRING('r', NULL, &root_dir,      "ROOTDIR" , _("Root directory for running container commands")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    struct dump_dir *dd = dd_opendir(dump_dir_name, /* for writing */0);
    if (dd == NULL)
        xfunc_die();

    char *container_cmdline = dd_load_text_ext(dd, FILENAME_CONTAINER_CMDLINE, DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
    if (container_cmdline == NULL)
        error_msg_and_die("The crash didn't occur in container");

    if (strstr("/docker ", container_cmdline) == 0)
        dump_docker_info(dd, root_dir);
    else if (strstr("/lxc-", container_cmdline) == 0)
        dump_lxc_info(dd, container_cmdline);
    else
        error_msg_and_die("Unsupported container technology");

    free(container_cmdline);
    dd_close(dd);

    return 0;
}
