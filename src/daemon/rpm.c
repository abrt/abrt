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
#include "libabrt.h"
#include "rpm.h"

/**
* A set, which contains finger prints.
*/

static GList *list_fingerprints = NULL;

/* cuts the name from the NVR format: foo-1.2.3-1.el6
   returns a newly allocated string
*/
char* get_package_name_from_NVR_or_NULL(const char* packageNVR)
{
    char* package_name = NULL;
    if (packageNVR != NULL)
    {
        log_notice("packageNVR %s", packageNVR);
        package_name = xstrdup(packageNVR);
        char *pos = strrchr(package_name, '-');
        if (pos != NULL)
        {
            *pos = '\0';
            pos = strrchr(package_name, '-');
            if (pos != NULL)
            {
                *pos = '\0';
            }
        }
    }
    return package_name;
}

void rpm_init()
{
    if (rpmReadConfigFiles(NULL, NULL) != 0)
        error_msg("Can't read RPM rc files");

    list_free_with_free(list_fingerprints); /* paranoia */
    /* Huh? Why do we start the list with an element with NULL string? */
    list_fingerprints = g_list_alloc();
}

void rpm_destroy()
{
    /* Mirroring the order of deinit calls in rpm-4.11.1/lib/poptALL.c::rpmcliFini() */
    rpmFreeCrypto();
    rpmFreeMacros(NULL);
    rpmFreeRpmrc();

    /* RPM doc says "clean up any open iterators and databases".
     * Observed to eliminate these Berkeley DB warnings:
     * "BDB2053 Freeing read locks for locker 0x1e0: 28718/139661746636736"
     */
    rpmdbCheckTerminate(1);

    list_free_with_free(list_fingerprints);
    list_fingerprints = NULL;
}

void rpm_load_gpgkey(const char* filename)
{
    uint8_t *pkt = NULL;
    size_t pklen;
    if (pgpReadPkts(filename, &pkt, &pklen) != PGPARMOR_PUBKEY)
    {
        free(pkt);
        error_msg("Can't load public GPG key %s", filename);
        return;
    }

    uint8_t keyID[8];
    if (pgpPubkeyFingerprint(pkt, pklen, keyID) == 0)
    {
        char *fingerprint = pgpHexStr(keyID, sizeof(keyID));
        if (fingerprint != NULL)
            list_fingerprints = g_list_append(list_fingerprints, fingerprint);
    }
    free(pkt);
}

int rpm_chk_fingerprint(const char* pkg)
{
    int ret = 0;
    char *pgpsig = NULL;
    const char *errmsg = NULL;

    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_NAME, pkg, 0);
    Header header = rpmdbNextIterator(iter);

    if (!header)
        goto error;

    pgpsig = headerFormat(header, "%|SIGGPG?{%{SIGGPG:pgpsig}}:{%{SIGPGP:pgpsig}}|", &errmsg);
    if (!pgpsig && errmsg)
    {
        log_notice("cannot get siggpg:pgpsig. reason: %s", errmsg);
        goto error;
    }

    {
        char *pgpsig_tmp = strstr(pgpsig, " Key ID ");
        if (pgpsig_tmp)
        {
            pgpsig_tmp += sizeof(" Key ID ") - 1;
            ret = g_list_find_custom(list_fingerprints, pgpsig_tmp, (GCompareFunc)g_strcmp0) != NULL;
        }
    }

error:
    free(pgpsig);
    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return ret;
}

/*
  Checking the MD5 sum requires to run prelink to "un-prelink" the
  binaries - this is considered potential security risk so we don't
  use it, until we find some non-intrusive way
*/

/*
 * Not woking, need to be rewriten
 *
bool CheckHash(const char* pPackage, const char* pPath)
{
    bool ret = true;
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_NAME, pPackage, 0);
    Header header = rpmdbNextIterator(iter);
    if (header == NULL)
        goto error;

    rpmfi fi = rpmfiNew(ts, header, RPMTAG_BASENAMES, RPMFI_NOHEADER);
    std::string headerHash;
    char computedHash[1024] = "";

    while (rpmfiNext(fi) != -1)
    {
        if (strcmp(pPath, rpmfiFN(fi)) == 0)
        {
            headerHash = rpmfiFDigestHex(fi, NULL);
            rpmDoDigest(rpmfiDigestAlgo(fi), pPath, 1, (unsigned char*) computedHash, NULL);
            ret = (headerHash != "" && headerHash == computedHash);
            break;
        }
    }
    rpmfiFree(fi);
error:
    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return ret;
}
*/

static int rpm_query_file(rpmts *ts, rpmdbMatchIterator *iter, Header *header,
        const char *filename, const char *rootdir_or_NULL)
{
    const char *queryname = filename;

    *ts = rpmtsCreate();
    if (rootdir_or_NULL)
    {
        if (rpmtsSetRootDir(*ts, rootdir_or_NULL) != 0)
        {
            rpmtsFree(*ts);
            return -1;
        }

        unsigned len = strlen(rootdir_or_NULL);
        /* remove 'chroot' prefix */
        if (strncmp(filename, rootdir_or_NULL, len) == 0 && filename[len] == '/')
            queryname += len;
    }

    *iter = rpmtsInitIterator(*ts, RPMTAG_BASENAMES, queryname, 0);
    *header = rpmdbNextIterator(*iter);

    if (!(*header) && rootdir_or_NULL)
    {
        rpmdbFreeIterator(*iter);
        rpmtsFree(*ts);

        return rpm_query_file(ts, iter, header, filename, NULL);
    }

    return 0;
}

char* rpm_get_component(const char *filename, const char *rootdir_or_NULL)
{
    char *ret = NULL;
    char *srpm = NULL;
    rpmts ts;
    rpmdbMatchIterator iter;
    Header header;

    if (rpm_query_file(&ts, &iter, &header, filename, rootdir_or_NULL) < 0)
        return NULL;

    if (!header)
        goto error;

    const char *errmsg = NULL;
    srpm = headerFormat(header, "%{SOURCERPM}", &errmsg);
    if (!srpm && errmsg)
    {
        error_msg("cannot get srpm. reason: %s", errmsg);
        goto error;
    }

    ret = get_package_name_from_NVR_or_NULL(srpm);
    free(srpm);

 error:
    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return ret;
}

#define pkg_add_id(name)                                                \
    static inline int pkg_add_##name(Header header, struct pkg_envra *p) \
    {                                                                   \
        const char *errmsg = NULL;                                      \
        p->p_##name = headerFormat(header, "%{"#name"}", &errmsg);      \
        if (p->p_##name || !errmsg)                                     \
            return 0;                                                   \
                                                                        \
        error_msg("cannot get "#name": %s", errmsg);                    \
                                                                        \
        return -1;                                                      \
    }                                                                   \

pkg_add_id(epoch);
pkg_add_id(name);
pkg_add_id(version);
pkg_add_id(release);
pkg_add_id(arch);

// caller is responsible to free returned value
struct pkg_envra *rpm_get_package_nvr(const char *filename, const char *rootdir_or_NULL)
{
    rpmts ts;
    rpmdbMatchIterator iter;
    Header header;

    struct pkg_envra *p = NULL;

    if (rpm_query_file(&ts, &iter, &header, filename, rootdir_or_NULL) < 0)
        return NULL;

    if (!header)
        goto error;

    p = xzalloc(sizeof(*p));
    int r;
    r = pkg_add_epoch(header, p);
    if (r)
        goto error;
   /*
    * <npajkovs> hello, what's the difference between epoch '0' and  '(none)'?
    * <Panu> nothing really, a missing epoch is considered equal to zero epoch
    */
    if (!strncmp(p->p_epoch, "(none)", strlen("(none)")))
    {
        free(p->p_epoch);
        p->p_epoch = xstrdup("0");
    }

    r = pkg_add_name(header, p);
    if (r)
        goto error;

    r = pkg_add_version(header, p);
    if (r)
        goto error;

    r = pkg_add_release(header, p);
    if (r)
        goto error;

    r = pkg_add_arch(header, p);
    if (r)
        goto error;

    p->p_nvr = xasprintf("%s-%s-%s", p->p_name, p->p_version, p->p_release);

    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return p;

 error:
    free_pkg_envra(p);

    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return NULL;
}

void free_pkg_envra(struct pkg_envra *p)
{
    if (!p)
        return;

    free(p->p_epoch);
    free(p->p_name);
    free(p->p_version);
    free(p->p_release);
    free(p->p_arch);
    free(p->p_nvr);
    free(p);
}
