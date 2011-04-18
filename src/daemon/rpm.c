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
#include "abrtlib.h"
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
        VERB1 log("packageNVR %s", packageNVR);
        package_name = xstrdup(packageNVR);
        char *pos = strrchr(package_name, '-');
        if (pos != NULL)
        {
            *pos = 0;
            pos = strrchr(package_name, '-');
            if (pos != NULL)
            {
                *pos = 0;
            }
        }
    }
    return package_name;
}

void rpm_init()
{
    int status = rpmReadConfigFiles((const char*)NULL, (const char*)NULL);
    if (status != 0)
        error_msg("error reading rc files");

    list_fingerprints = g_list_alloc();
}

static void list_free(gpointer data, gpointer user_data)
{
    free(data);
}

void rpm_destroy()
{
    rpmFreeRpmrc();
    rpmFreeCrypto();
    rpmFreeMacros(NULL);

    g_list_foreach(list_fingerprints, list_free, NULL);
    g_list_free(list_fingerprints);
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
        VERB1 log("cannot get siggpg:pgpsig. reason: %s", errmsg);
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

char* rpm_get_component(const char* filename)
{
    char *ret = NULL;
    char *srpm = NULL;
    const char *errmsg = NULL;

    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_BASENAMES, filename, 0);
    Header header = rpmdbNextIterator(iter);
    if (!header)
        goto error;

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

// caller is responsible to free returned value
char* rpm_get_package_nvr(const char* filename)
{
    char* nvr = NULL;
    const char *errmsg = NULL;

    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_BASENAMES, filename, 0);
    Header header = rpmdbNextIterator(iter);

    if (!header)
        goto error;

    nvr = headerFormat(header, "%{NAME}-%{VERSION}-%{RELEASE}", &errmsg);
    if (!nvr && errmsg)
        error_msg("cannot get nvr. reason: %s", errmsg);

error:
    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return nvr;
}
