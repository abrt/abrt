/*
    RPM.h - header file for rpm database
              - it implements query for local rpm database

    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
    Copyright (C) 2009  RedHat inc.

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
#ifndef RPM_H_
#define RPM_H_

#ifdef __cplusplus
extern "C" {
#endif

struct pkg_nevra {
    char *p_nvr;
    char *p_name;
    char *p_epoch;
    char *p_version;
    char *p_release;
    char *p_arch;
    char *p_vendor;
};

void free_pkg_nevra(struct pkg_nevra *p);

/**
 * Checks if an application is modified by third party.
 * @param pPackage A package name. The package contains the application.
 * @param pPath A path to the application.
 *
 * Not used. Delete?
 */
//bool CheckHash(const char* pPackage, const char* pPath);

void rpm_init();

void rpm_destroy();

/**
 * A function, which loads one GPG public key.
 * @param filename A path to the public key.
 */
void rpm_load_gpgkey(const char* filename);

/**
 * A function, which checks if package's finger print is valid.
 * @param pkg A package name.
 * @return 1 if valid, otherwise (invalid, or error) 0
 */
int rpm_chk_fingerprint(const char* pkg);

/**
 * A function, which checks if the given finger print is imported.
 * @param pkg A package name.
 * @return 1 if imported, otherwise (not-imported, or error) 0
 */
int rpm_fingerprint_is_imported(const char* fingerprint);

/**
 * A function, which returns package's finger print
 * @param pkg A package name.
 * @return NULL if not-valid, otherwise malloced NULL-terminated string.
 */
char *rpm_get_fingerprint(const char* pkg);

/**
 * Gets a package name. This package contains particular
 * file. If the file doesn't belong to any package, empty string is
 * returned.
 * @param filename A file name.
 * @return A package name (malloc'ed string)
 */
struct pkg_nevra *rpm_get_package_nvr(const char *filename, const char *rootdir_or_NULL);
/**
 * Finds a main package for given file. This package contains particular
 * file. If the file doesn't belong to any package, empty string is
 * returned.
 * @param filename A file name.
 * @return Component name (malloc'ed string)
 */
char* rpm_get_component(const char *filename, const char *rootdir_or_NULL);

char* get_package_name_from_NVR_or_NULL(const char* packageNVR);

#ifdef __cplusplus
}
#endif

#endif
