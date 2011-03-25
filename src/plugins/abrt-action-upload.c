/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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

#include <libtar.h>
#include <curl/curl.h>
#include "abrtlib.h"
#include "parse_options.h"

#define PROGNAME "abrt-action-upload"

//TODO: use this for better logging
#if 0
/* "read local data from a file" callback */
static size_t fread_with_reporting(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    static time_t last_t; // hack

    FILE *fp = (FILE*)userdata;
    time_t t = time(NULL);

    // Report current file position every 16 seconds
    if (!(t & 0xf) && last_t != t)
    {
        last_t = t;
        off_t cur_pos = ftello(fp);
        fseeko(fp, 0, SEEK_END);
        off_t sz = ftello(fp);
        fseeko(fp, cur_pos, SEEK_SET);
        log(_("Uploaded: %llu of %llu kbytes"),
                (unsigned long long)cur_pos / 1024,
                (unsigned long long)sz / 1024);
    }

    return fread(ptr, size, nmemb, fp);
}
#endif

static int send_file(const char *url, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        perror_msg("Can't open '%s'", filename);
        return 1;
    }

    log(_("Sending %s to %s"), filename, url);

    struct stat stbuf;
    fstat(fileno(fp), &stbuf); /* never fails */
    char *whole_url = concat_path_file(url, strrchr(filename, '/') ? : filename);

    CURL *curl = curl_easy_init();
    if (!curl)
    {
        error_msg_and_die("Can't create curl handle");
    }
    /* Buffer[CURL_ERROR_SIZE] curl stores human readable error messages in.
     * This may be more helpful than just return code from curl_easy_perform.
     * curl will need it until curl_easy_cleanup. */
    char curl_err_msg[CURL_ERROR_SIZE];
    curl_err_msg[0] = '\0';
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_err_msg);
    /* enable uploading */
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    /* specify target */
    curl_easy_setopt(curl, CURLOPT_URL, whole_url);
    /* FILE handle: passed to the default callback, it will fread() it */
    curl_easy_setopt(curl, CURLOPT_READDATA, fp);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)stbuf.st_size);

    /* everything is done here; result 0 means success */
    CURLcode result = curl_easy_perform(curl);
    free(whole_url);
    fclose(fp);
    if (result != 0)
        error_msg("Error while uploading: '%s'", curl_easy_strerror(result));
    else
        /* This ends up a "reporting status message" in abrtd */
        log(_("Successfully sent %s to %s"), filename, url);

    curl_easy_cleanup(curl);

    return result;
}

static int create_and_upload_archive(
                const char *dump_dir_name,
                map_string_h *settings)
{
    int result = 0;

    pid_t child;
    TAR* tar = NULL;
    const char* errmsg = NULL;
    char* tempfile = NULL;

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        exit(1); /* error msg is already logged by dd_opendir */

    /* Gzipping e.g. 0.5gig coredump takes a while. Let client know what we are doing */
    log(_("Compressing data"));

//TODO:
//Encrypt = yes
//ArchiveType = .tar.bz2
//ExcludeFiles = foo,bar*,b*z
    char* env;
    env = getenv("Upload_URL");
    const char *url = (env ? env : get_map_string_item_or_empty(settings, "URL"));

    /* Create a child gzip which will compress the data */
    /* SELinux guys are not happy with /tmp, using /var/run/abrt */
    tempfile = xasprintf(LOCALSTATEDIR"/run/abrt/upload-%s-%lu.tar.gz", iso_date_string(NULL), (long)getpid());
    int pipe_from_parent_to_child[2];
    xpipe(pipe_from_parent_to_child);
    child = fork();
    if (child == 0)
    {
        /* child */
        close(pipe_from_parent_to_child[1]);
        xmove_fd(pipe_from_parent_to_child[0], 0);
        xmove_fd(xopen3(tempfile, O_WRONLY | O_CREAT | O_EXCL, 0600), 1);
        execlp("gzip", "gzip", NULL);
        perror_msg_and_die("can't execute '%s'", "gzip");
    }
    close(pipe_from_parent_to_child[0]);

    /* Create tar writer object */
    if (tar_fdopen(&tar, pipe_from_parent_to_child[1], tempfile,
                /*fileops:(standard)*/ NULL, O_WRONLY | O_CREAT, 0644, TAR_GNU) != 0)
    {
        errmsg = "Can't create temporary file in "LOCALSTATEDIR"/run/abrt";
        goto ret;
    }

    /* Write data to the tarball */
    {
        dd_init_next_file(dd);
        char *short_name, *full_name;
        while (dd_get_next_file(dd, &short_name, &full_name))
        {
            if (strcmp(short_name, FILENAME_COUNT) == 0) goto next;
            if (strcmp(short_name, CD_DUMPDIR) == 0) goto next;
            // dd_get_next_file guarantees this:
            //struct stat stbuf;
            //if (stat(full_name, &stbuf) != 0)
            // || !S_ISREG(stbuf.st_mode)
            //) {
            //     goto next;
            //}
            if (tar_append_file(tar, full_name, short_name) != 0)
            {
                errmsg = "Can't create temporary file in "LOCALSTATEDIR"/run/abrt";
                free(short_name);
                free(full_name);
                goto ret;
            }
 next:
            free(short_name);
            free(full_name);
        }
    }
    dd_close(dd);
    dd = NULL;

    /* Close tar writer... */
    if (tar_append_eof(tar) != 0 || tar_close(tar) != 0)
    {
        errmsg = "Can't create temporary file in "LOCALSTATEDIR"/run/abrt";
        goto ret;
    }
    tar = NULL;
    /* ...and check that gzip child finished successfully */
    int status;
    waitpid(child, &status, 0);
    child = -1;
    if (status != 0)
    {
        /* We assume the error was out-of-disk-space or out-of-quota */
        errmsg = "Can't create temporary file in "LOCALSTATEDIR"/run/abrt";
        goto ret;
    }

    /* Upload the tarball */
    if (url && url[0])
    {
        result = send_file(url, tempfile);
        /* cleanup code will delete tempfile */
    }
    else
    {
        log(_("Archive is created: '%s'"), tempfile);
        free(tempfile);
        tempfile = NULL;
    }

 ret:
    dd_close(dd);
    if (tar)
        tar_close(tar);
    /* close(pipe_from_parent_to_child[1]); - tar_close() does it itself */
    if (child > 0)
        waitpid(child, NULL, 0);
    if (tempfile)
    {
        unlink(tempfile);
        free(tempfile);
    }
    if (errmsg)
        error_msg_and_die("%s", errmsg);

    return result;
}

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    const char *dump_dir_name = ".";
    const char *conf_file = NULL;
    const char *url = NULL;

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        PROGNAME" [-v] -d DIR [-c CONFFILE] [-u URL]\n"
        "\n"
        "Uploads compressed tarball of dump directory DIR"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
        OPT_c = 1 << 2,
        OPT_u = 1 << 3,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR"     , _("Dump directory")),
        OPT_STRING('c', NULL, &conf_file    , "CONFFILE", _("Config file")),
        OPT_STRING('u', NULL, &url          , "URL"     , _("Base URL to upload to")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));

    char *pfx = getenv("ABRT_PROG_PREFIX");
    if (pfx && string_to_bool(pfx))
        msg_prefix = PROGNAME;

    map_string_h *settings = new_map_string();
    if (url)
        g_hash_table_replace(settings, xstrdup("URL"), xstrdup(url));
    if (conf_file)
        load_conf_file(conf_file, settings, /*skip key w/o values:*/ true);

    int result = create_and_upload_archive(dump_dir_name, settings);

    free_map_string(settings);
    return result;
}
